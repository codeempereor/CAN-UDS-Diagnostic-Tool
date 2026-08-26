#ifdef HAS_SOCKETCAN

#ifndef SOCKET_CAN_ADAPTER_H
#define SOCKET_CAN_ADAPTER_H

#include "can_adapter_base.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <string>
#include <cstdint>

class SocketCanAdapter : public CanAdapterBase {
public:
    SocketCanAdapter();
    ~SocketCanAdapter() override;

    bool open(const std::string& interface, uint32_t bitrate = 500000) override;
    void close() override;
    bool sendFrame(const CanFrame& frame) override;

    bool isOpen() const override;
    std::string adapterName() const override;

private:
    void receiveThread();

    int m_socketFd;
    std::atomic<bool> m_running;
    std::thread m_receiveThread;
    std::mutex m_mutex;
    std::string m_interface;
};

#endif // SOCKET_CAN_ADAPTER_H

#endif // HAS_SOCKETCAN
