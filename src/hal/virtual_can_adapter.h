#ifndef VIRTUAL_CAN_ADAPTER_H
#define VIRTUAL_CAN_ADAPTER_H

#include "can_adapter_base.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <queue>
#include <cstdint>

// 虚拟CAN适配器：用于无硬件环境下的开发测试
// 支持自发自收模式，模拟ECU响应
class VirtualCanAdapter : public CanAdapterBase {
public:
    VirtualCanAdapter();
    ~VirtualCanAdapter() override;

    bool open(const std::string& interface, uint32_t bitrate = 500000) override;
    void close() override;
    bool sendFrame(const CanFrame& frame) override;

    bool isOpen() const override;
    std::string adapterName() const override;

    // 启用模拟ECU模式：自动响应UDS请求
    void setSimulateEcu(bool enable);
    bool simulateEcu() const;

private:
    void receiveThread();
    void simulateEcuResponse(const CanFrame& request);

    std::atomic<bool> m_running;
    std::thread m_receiveThread;
    std::mutex m_mutex;

    bool m_simulateEcu;
    uint32_t m_ecuTxId;
    uint32_t m_ecuRxId;
    uint8_t m_currentSession;

    // 模拟DID数据
    std::vector<uint8_t> getSimulatedDidData(uint16_t did);
};

#endif // VIRTUAL_CAN_ADAPTER_H
