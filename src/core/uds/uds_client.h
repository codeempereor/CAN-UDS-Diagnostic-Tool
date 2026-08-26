#ifndef UDS_CLIENT_H
#define UDS_CLIENT_H

#include "uds_types.h"
#include "isotp/isotp_client.h"
#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include <cstdint>

enum class UdsErrorType {
    None,
    RequestTimeout,         // 请求超时（ECU未在规定时间内响应）
    ResponseMismatch,       // 响应服务ID与请求不匹配
    IsoTpError              // ISO-TP层错误（透传）
};

enum class SecurityAccessState {
    Idle,                   // 未进行安全访问
    WaitingForSeed,         // 已请求种子，等待ECU返回
    WaitingForKeyResponse,  // 已发送密钥，等待ECU确认
    Unlocked                // 已解锁
};

struct PendingRequest {
    uint8_t serviceId;
    uint64_t sendTimeUs;
    bool valid;
};

// 密钥计算回调：输入种子，输出密钥
using KeyCalculator = std::function<std::vector<uint8_t>(const std::vector<uint8_t>& seed)>;
// 安全访问完成回调：success表示是否解锁成功，unlockedLevel表示解锁的level
using SecurityAccessCallback = std::function<void(bool success, uint8_t unlockedLevel)>;

class UdsClient {
public:
    using ResponseCallback = std::function<void(const UdsResponse& response)>;
    using SendFrameCallback = std::function<void(const CanFrame& frame)>;
    using ErrorCallback = std::function<void(IsoTpErrorType error)>;
    using UdsErrorCallback = std::function<void(UdsErrorType error, uint8_t serviceId)>;

    UdsClient();
    ~UdsClient();

    void setTxId(uint32_t id);
    void setRxId(uint32_t id);
    void setExtendedFrame(bool extended);

    void setSendFrameCallback(SendFrameCallback callback);
    void setResponseCallback(ResponseCallback callback);
    void setErrorCallback(ErrorCallback callback);
    void setUdsErrorCallback(UdsErrorCallback callback);

    // 传入底层收到的CAN帧
    void handleReceivedFrame(const CanFrame& frame);

    // 超时检查（外部需定期调用，建议每100ms一次）
    // 同时检查ISO-TP超时和UDS请求超时
    bool checkTimeout();

    // 是否有pending请求
    bool hasPendingRequest() const;

    // 设置请求超时时间（微秒），默认3秒
    void setRequestTimeoutUs(uint32_t timeoutUs);

    // 核心诊断服务
    bool diagnosticSessionControl(UdsSessionType sessionType);
    bool readDataByIdentifier(uint16_t did);
    bool writeDataByIdentifier(uint16_t did, const std::vector<uint8_t>& data);
    bool routineControl(RoutineControlType type, uint16_t rid, const std::vector<uint8_t>& params = {});
    bool ecuReset(uint8_t resetType = 0x01);
    bool testerPresent(bool suppressPositiveResponse = false);
    bool securityAccessRequestSeed(uint8_t level);
    bool securityAccessSendKey(uint8_t level, const std::vector<uint8_t>& key);

    // 完整安全访问流程：请求种子 → 计算密钥 → 发送密钥
    // level为请求种子的level（奇数），keyCalculator为密钥计算函数
    bool startSecurityAccess(uint8_t level, KeyCalculator keyCalculator, SecurityAccessCallback callback = nullptr);

    // 安全访问状态查询
    SecurityAccessState securityAccessState() const;
    uint8_t unlockedLevel() const;
    bool isSecurityUnlocked(uint8_t level = 0) const;

    // 通用请求
    bool sendRequest(uint8_t serviceId, const std::vector<uint8_t>& data = {});

    void reset();

    UdsSessionType currentSession() const;

private:
    void onIsoTpReceived(const std::vector<uint8_t>& data);
    void parseResponse(const std::vector<uint8_t>& data);

    IsoTpClient m_isoTp;
    ResponseCallback m_responseCallback;
    UdsErrorCallback m_udsErrorCallback;

    UdsSessionType m_currentSession;
    mutable std::recursive_mutex m_mutex;

    uint32_t m_txId;
    uint32_t m_rxId;
    bool m_extended;

    // Pending请求（UDS半双工，同一时间只有一个）
    PendingRequest m_pendingRequest;
    uint32_t m_requestTimeoutUs;
    static constexpr uint32_t DEFAULT_REQUEST_TIMEOUT_US = 3000000; // 3秒

    // 安全访问状态
    SecurityAccessState m_securityState;
    uint8_t m_securityLevel;       // 当前请求的安全访问level
    uint8_t m_unlockedLevel;       // 已解锁的level（0表示未解锁）
    KeyCalculator m_keyCalculator;
    SecurityAccessCallback m_securityCallback;
};

#endif // UDS_CLIENT_H
