#ifdef HAS_SOCKETCAN

#include "hal/socket_can_adapter.h"
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>

SocketCanAdapter::SocketCanAdapter()
    : m_socketFd(-1)
    , m_running(false)
{
}

SocketCanAdapter::~SocketCanAdapter()
{
    close();
}

bool SocketCanAdapter::open(const std::string& interface, uint32_t bitrate)
{
    (void)bitrate; // bitrate通过ip link命令设置，这里不处理

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running) return true;

    setState(CanAdapterState::Opening);
    m_interface = interface;

    m_socketFd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (m_socketFd < 0) {
        notifyError("Failed to create socket: " + std::string(strerror(errno)));
        setState(CanAdapterState::Error);
        return false;
    }

    struct ifreq ifr;
    std::strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    if (ioctl(m_socketFd, SIOCGIFINDEX, &ifr) < 0) {
        notifyError("Failed to get interface index: " + std::string(strerror(errno)));
        ::close(m_socketFd);
        m_socketFd = -1;
        setState(CanAdapterState::Error);
        return false;
    }

    struct sockaddr_can addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(m_socketFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        notifyError("Failed to bind socket: " + std::string(strerror(errno)));
        ::close(m_socketFd);
        m_socketFd = -1;
        setState(CanAdapterState::Error);
        return false;
    }

    m_running = true;
    m_receiveThread = std::thread([this]() {
        receiveThread();
    });

    setState(CanAdapterState::Open);
    return true;
}

void SocketCanAdapter::close()
{
    m_running = false;

    if (m_socketFd >= 0) {
        ::close(m_socketFd);
        m_socketFd = -1;
    }

    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }

    setState(CanAdapterState::Closed);
}

bool SocketCanAdapter::sendFrame(const CanFrame& frame)
{
    if (!m_running || m_socketFd < 0) return false;

    struct can_frame canFrame;
    std::memset(&canFrame, 0, sizeof(canFrame));

    canFrame.can_id = frame.id;
    if (frame.extended) {
        canFrame.can_id |= CAN_EFF_FLAG;
    }
    if (frame.remote) {
        canFrame.can_id |= CAN_RTR_FLAG;
    }
    if (frame.error) {
        canFrame.can_id |= CAN_ERR_FLAG;
    }

    canFrame.can_dlc = frame.dlc;
    std::memcpy(canFrame.data, frame.data.data(), std::min<uint8_t>(frame.dlc, 8));

    ssize_t bytesWritten = write(m_socketFd, &canFrame, sizeof(canFrame));
    return bytesWritten == sizeof(canFrame);
}

bool SocketCanAdapter::isOpen() const
{
    return m_running.load();
}

std::string SocketCanAdapter::adapterName() const
{
    return "SocketCAN: " + m_interface;
}

void SocketCanAdapter::receiveThread()
{
    struct can_frame canFrame;

    while (m_running) {
        ssize_t nbytes = read(m_socketFd, &canFrame, sizeof(canFrame));
        if (nbytes != sizeof(canFrame)) {
            if (!m_running) break;
            continue;
        }

        CanFrame frame;
        frame.id = canFrame.can_id & CAN_EFF_MASK;
        frame.extended = (canFrame.can_id & CAN_EFF_FLAG) != 0;
        frame.remote = (canFrame.can_id & CAN_RTR_FLAG) != 0;
        frame.error = (canFrame.can_id & CAN_ERR_FLAG) != 0;
        frame.dlc = canFrame.can_dlc;
        frame.data.assign(canFrame.data, canFrame.data + canFrame.can_dlc);
        frame.timestamp_us = CanFrame::currentTimestampUs();

        notifyReceived(frame);
    }
}

#endif // HAS_SOCKETCAN
