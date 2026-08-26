#ifndef ISOTP_CLIENT_H
#define ISOTP_CLIENT_H

#include "can/can_frame.h"
#include <vector>
#include <cstdint>
#include <functional>
#include <mutex>
#include <chrono>

enum class IsoTpState {
    Idle,
    Sending,
    Receiving
};

enum class IsoTpFrameType : uint8_t {
    SingleFrame = 0x00,
    FirstFrame = 0x01,
    ConsecutiveFrame = 0x02,
    FlowControlFrame = 0x03
};

enum class FlowControlFlag : uint8_t {
    ContinueToSend = 0x00,
    Wait = 0x01,
    Overflow = 0x02
};

enum class IsoTpErrorType {
    None,
    TxFlowControlTimeout,   // 发送方等待流控帧超时
    RxConsecutiveTimeout,   // 接收方等待连续帧超时
    SequenceError,          // 连续帧序列号错误
    Overflow                // 接收方缓冲区溢出
};

class IsoTpClient {
public:
    using ReceiveCallback = std::function<void(const std::vector<uint8_t>& data)>;
    using SendFrameCallback = std::function<void(const CanFrame& frame)>;
    using ErrorCallback = std::function<void(IsoTpErrorType error)>;

    IsoTpClient();
    ~IsoTpClient();

    void setTxId(uint32_t id);
    void setRxId(uint32_t id);
    void setExtendedFrame(bool extended);

    void setReceiveCallback(ReceiveCallback callback);
    void setSendFrameCallback(SendFrameCallback callback);
    void setErrorCallback(ErrorCallback callback);

    // 发送任意长度数据（自动分片）
    bool sendData(const std::vector<uint8_t>& data);

    // 接收CAN帧（传入底层收到的CAN帧，内部自动重组）
    void handleReceivedFrame(const CanFrame& frame);

    // 流控参数配置
    void setBlockSize(uint8_t bs);
    void setStMin(uint8_t st_min);

    // 超时检查（外部需定期调用，建议每100ms调用一次）
    // 返回true表示发生了超时并已重置
    bool checkTimeout();

    void reset();

    IsoTpState state() const;

private:
    bool sendSingleFrame(const std::vector<uint8_t>& data);
    bool sendFirstFrame(uint32_t totalLen, const std::vector<uint8_t>& firstData);
    void sendConsecutiveFrames();
    void sendFlowControl(FlowControlFlag flag, uint8_t bs, uint8_t st_min);

    void handleSingleFrame(const CanFrame& frame);
    void handleFirstFrame(const CanFrame& frame);
    void handleConsecutiveFrame(const CanFrame& frame);
    void handleFlowControlFrame(const CanFrame& frame);

    uint32_t m_txId;
    uint32_t m_rxId;
    bool m_extended;

    ReceiveCallback m_receiveCallback;
    SendFrameCallback m_sendFrameCallback;
    ErrorCallback m_errorCallback;

    // 发送状态
    std::vector<uint8_t> m_txBuffer;
    size_t m_txIndex;
    uint8_t m_txSequence;
    uint8_t m_blockSize;
    uint8_t m_stMin;
    uint8_t m_remainingInBlock;
    bool m_waitingForFlowControl;

    // 接收状态
    std::vector<uint8_t> m_rxBuffer;
    uint32_t m_rxTotalLength;
    size_t m_rxReceived;
    uint8_t m_rxSequence;
    uint8_t m_rxBlockSize;
    uint8_t m_rxStMin;

    IsoTpState m_state;
    mutable std::recursive_mutex m_mutex;

    // 超时
    uint64_t m_lastFrameTimeUs;
    static constexpr uint32_t TIMEOUT_US = 1000000; // 1秒超时
};

#endif // ISOTP_CLIENT_H
