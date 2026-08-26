#include "hal/virtual_can_adapter.h"
#include <chrono>
#include <cstring>

VirtualCanAdapter::VirtualCanAdapter()
    : m_running(false)
    , m_simulateEcu(true)
    , m_ecuTxId(0x7E8)
    , m_ecuRxId(0x7E0)
    , m_currentSession(0x01)
{
}

VirtualCanAdapter::~VirtualCanAdapter()
{
    close();
}

bool VirtualCanAdapter::open(const std::string& interface, uint32_t bitrate)
{
    (void)interface;
    (void)bitrate;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running) return true;

    setState(CanAdapterState::Opening);
    m_running = true;

    m_receiveThread = std::thread([this]() {
        receiveThread();
    });

    setState(CanAdapterState::Open);
    return true;
}

void VirtualCanAdapter::close()
{
    m_running = false;
    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }
    setState(CanAdapterState::Closed);
}

bool VirtualCanAdapter::sendFrame(const CanFrame& frame)
{
    if (!m_running) return false;

    // 模拟ECU响应
    if (m_simulateEcu && frame.id == m_ecuRxId) {
        simulateEcuResponse(frame);
    }

    return true;
}

bool VirtualCanAdapter::isOpen() const
{
    return m_running.load();
}

std::string VirtualCanAdapter::adapterName() const
{
    return "Virtual CAN Adapter";
}

void VirtualCanAdapter::setSimulateEcu(bool enable)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_simulateEcu = enable;
}

bool VirtualCanAdapter::simulateEcu() const
{
    return m_simulateEcu;
}

void VirtualCanAdapter::receiveThread()
{
    // 虚拟适配器的接收线程主要用于定时发送一些模拟报文
    uint32_t counter = 0;

    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (!m_simulateEcu) continue;

        // 模拟周期报文 0x100
        CanFrame periodicFrame;
        periodicFrame.id = 0x100;
        periodicFrame.dlc = 8;
        periodicFrame.data.resize(8);
        periodicFrame.data[0] = static_cast<uint8_t>(counter & 0xFF);
        periodicFrame.data[1] = static_cast<uint8_t>((counter >> 8) & 0xFF);
        periodicFrame.data[2] = 0x12;
        periodicFrame.data[3] = 0x34;
        periodicFrame.data[4] = 0x56;
        periodicFrame.data[5] = 0x78;
        periodicFrame.data[6] = 0x9A;
        periodicFrame.data[7] = 0xBC;
        periodicFrame.timestamp_us = CanFrame::currentTimestampUs();

        notifyReceived(periodicFrame);

        // 模拟周期报文 0x200
        CanFrame frame2;
        frame2.id = 0x200;
        frame2.dlc = 8;
        frame2.data.resize(8);
        uint16_t speed = 500 + (counter % 100); // 模拟转速变化
        frame2.data[0] = static_cast<uint8_t>((speed >> 8) & 0xFF);
        frame2.data[1] = static_cast<uint8_t>(speed & 0xFF);
        uint16_t temp = 800 + (counter % 50); // 模拟温度
        frame2.data[2] = static_cast<uint8_t>((temp >> 8) & 0xFF);
        frame2.data[3] = static_cast<uint8_t>(temp & 0xFF);
        frame2.data[4] = 0x00;
        frame2.data[5] = 0x00;
        frame2.data[6] = 0x00;
        frame2.data[7] = 0x00;
        frame2.timestamp_us = CanFrame::currentTimestampUs();

        notifyReceived(frame2);

        counter++;
    }
}

void VirtualCanAdapter::simulateEcuResponse(const CanFrame& request)
{
    if (request.dlc < 1) return;

    uint8_t pci = request.data[0];
    uint8_t frameType = (pci >> 4) & 0x0F;

    // 只处理单帧UDS请求
    if (frameType != 0) return;

    uint8_t len = pci & 0x0F;
    if (len < 1 || len > request.dlc - 1) return;

    uint8_t serviceId = request.data[1];

    CanFrame response;
    response.id = m_ecuTxId;
    response.extended = request.extended;
    response.timestamp_us = CanFrame::currentTimestampUs();

    switch (serviceId) {
    case 0x10: { // 会话控制
        if (len >= 2) {
            m_currentSession = request.data[2];
            response.dlc = 2;
            response.data.resize(2);
            response.data[0] = 0x02;
            response.data[1] = 0x50; // 0x10 + 0x40
            response.data.push_back(request.data[2]); // 回显会话类型
            response.dlc = 3;
        }
        break;
    }
    case 0x22: { // 读DID
        if (len >= 3) {
            uint16_t did = (request.data[2] << 8) | request.data[3];
            std::vector<uint8_t> didData = getSimulatedDidData(did);

            // 构造响应
            std::vector<uint8_t> respData;
            respData.push_back(0x62); // 0x22 + 0x40
            respData.push_back(request.data[2]);
            respData.push_back(request.data[3]);
            respData.insert(respData.end(), didData.begin(), didData.end());

            if (respData.size() <= 7) {
                // 单帧响应
                response.dlc = static_cast<uint8_t>(respData.size() + 1);
                response.data.resize(response.dlc);
                response.data[0] = static_cast<uint8_t>(respData.size());
                std::memcpy(response.data.data() + 1, respData.data(), respData.size());
            } else {
                // 首帧响应（简化处理，测试用）
                response.dlc = 8;
                response.data.resize(8);
                uint16_t totalLen = static_cast<uint16_t>(respData.size());
                response.data[0] = 0x10 | static_cast<uint8_t>((totalLen >> 8) & 0x0F);
                response.data[1] = static_cast<uint8_t>(totalLen & 0xFF);
                size_t copyLen = std::min<size_t>(6, respData.size());
                std::memcpy(response.data.data() + 2, respData.data(), copyLen);
            }
        }
        break;
    }
    case 0x2E: { // 写DID
        if (len >= 3) {
            response.dlc = 4;
            response.data.resize(4);
            response.data[0] = 0x03;
            response.data[1] = 0x6E; // 0x2E + 0x40
            response.data[2] = request.data[2];
            response.data[3] = request.data[3];
        }
        break;
    }
    case 0x31: { // 例程控制
        if (len >= 4) {
            response.dlc = 5;
            response.data.resize(5);
            response.data[0] = 0x04;
            response.data[1] = 0x71; // 0x31 + 0x40
            response.data[2] = request.data[2];
            response.data[3] = request.data[3];
            response.data[4] = request.data[4];
        }
        break;
    }
    case 0x3E: { // TesterPresent
        response.dlc = 2;
        response.data.resize(2);
        response.data[0] = 0x01;
        response.data[1] = 0x7E; // 0x3E + 0x40
        break;
    }
    case 0x11: { // ECU复位
        response.dlc = 2;
        response.data.resize(2);
        response.data[0] = 0x01;
        response.data[1] = 0x51; // 0x11 + 0x40
        break;
    }
    default: {
        // 不支持的服务，返回否定响应
        response.dlc = 3;
        response.data.resize(3);
        response.data[0] = 0x03;
        response.data[1] = 0x7F;
        response.data[2] = serviceId;
        response.data.push_back(0x11); // serviceNotSupported
        response.dlc = 4;
        break;
    }
    }

    if (response.dlc > 0) {
        // 延迟一点模拟真实响应
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        notifyReceived(response);
    }
}

std::vector<uint8_t> VirtualCanAdapter::getSimulatedDidData(uint16_t did)
{
    std::vector<uint8_t> data;

    switch (did) {
    case 0xF190: // VIN
        data = {'T', 'E', 'S', 'T', 'V', 'I', 'N', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};
        break;
    case 0xF18E: // 硬件版本
        data = {'H', 'W', '_', 'V', '1', '.', '0', '.', '0'};
        break;
    case 0xF18A: // 软件版本
        data = {'S', 'W', '_', 'V', '2', '.', '3', '.', '1'};
        break;
    case 0xF18C: // 供应商
        data = {'V', 'I', 'R', 'T', 'U', 'A', 'L'};
        break;
    case 0x1234: // 自定义数据
        data.push_back(0x12);
        data.push_back(0x34);
        data.push_back(0x56);
        data.push_back(0x78);
        break;
    default:
        data.push_back(0x00);
        data.push_back(0x00);
        break;
    }

    return data;
}
