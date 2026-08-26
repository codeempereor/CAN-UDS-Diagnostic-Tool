#include "uds/uds_client.h"
#include <cstring>

UdsClient::UdsClient()
    : m_currentSession(UdsSessionType::DefaultSession)
    , m_txId(0x7E0)
    , m_rxId(0x7E8)
    , m_extended(false)
    , m_pendingRequest{0, 0, false}
    , m_requestTimeoutUs(DEFAULT_REQUEST_TIMEOUT_US)
    , m_securityState(SecurityAccessState::Idle)
    , m_securityLevel(0)
    , m_unlockedLevel(0)
{
    m_isoTp.setReceiveCallback([this](const std::vector<uint8_t>& data) {
        onIsoTpReceived(data);
    });
}

UdsClient::~UdsClient()
{
}

void UdsClient::setTxId(uint32_t id)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_txId = id;
    m_isoTp.setTxId(id);
}

void UdsClient::setRxId(uint32_t id)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_rxId = id;
    m_isoTp.setRxId(id);
}

void UdsClient::setExtendedFrame(bool extended)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_extended = extended;
    m_isoTp.setExtendedFrame(extended);
}

void UdsClient::setSendFrameCallback(SendFrameCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_isoTp.setSendFrameCallback(callback);
}

void UdsClient::setResponseCallback(ResponseCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_responseCallback = callback;
}

void UdsClient::setErrorCallback(ErrorCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_isoTp.setErrorCallback(callback);
}

void UdsClient::setUdsErrorCallback(UdsErrorCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_udsErrorCallback = callback;
}

bool UdsClient::hasPendingRequest() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_pendingRequest.valid;
}

void UdsClient::setRequestTimeoutUs(uint32_t timeoutUs)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_requestTimeoutUs = timeoutUs;
}

void UdsClient::handleReceivedFrame(const CanFrame& frame)
{
    m_isoTp.handleReceivedFrame(frame);
}

bool UdsClient::checkTimeout()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // 1. 先检查ISO-TP层超时
    bool isotpTimedOut = m_isoTp.checkTimeout();

    // 2. 检查UDS请求超时
    if (m_pendingRequest.valid) {
        uint64_t now = CanFrame::currentTimestampUs();
        uint64_t elapsed = now - m_pendingRequest.sendTimeUs;
        if (elapsed >= m_requestTimeoutUs) {
            uint8_t serviceId = m_pendingRequest.serviceId;
            m_pendingRequest.valid = false;
            UdsErrorCallback cb = m_udsErrorCallback;
            if (cb) {
                cb(UdsErrorType::RequestTimeout, serviceId);
            }
            return true;
        }
    }

    return isotpTimedOut;
}

bool UdsClient::diagnosticSessionControl(UdsSessionType sessionType)
{
    std::vector<uint8_t> data;
    data.push_back(static_cast<uint8_t>(sessionType));
    return sendRequest(static_cast<uint8_t>(UdsService::DiagnosticSessionControl), data);
}

bool UdsClient::readDataByIdentifier(uint16_t did)
{
    std::vector<uint8_t> data;
    data.push_back(static_cast<uint8_t>((did >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(did & 0xFF));
    return sendRequest(static_cast<uint8_t>(UdsService::ReadDataByIdentifier), data);
}

bool UdsClient::writeDataByIdentifier(uint16_t did, const std::vector<uint8_t>& data)
{
    std::vector<uint8_t> req;
    req.push_back(static_cast<uint8_t>((did >> 8) & 0xFF));
    req.push_back(static_cast<uint8_t>(did & 0xFF));
    req.insert(req.end(), data.begin(), data.end());
    return sendRequest(static_cast<uint8_t>(UdsService::WriteDataByIdentifier), req);
}

bool UdsClient::routineControl(RoutineControlType type, uint16_t rid, const std::vector<uint8_t>& params)
{
    std::vector<uint8_t> req;
    req.push_back(static_cast<uint8_t>(type));
    req.push_back(static_cast<uint8_t>((rid >> 8) & 0xFF));
    req.push_back(static_cast<uint8_t>(rid & 0xFF));
    req.insert(req.end(), params.begin(), params.end());
    return sendRequest(static_cast<uint8_t>(UdsService::RoutineControl), req);
}

bool UdsClient::ecuReset(uint8_t resetType)
{
    std::vector<uint8_t> data;
    data.push_back(resetType);
    return sendRequest(static_cast<uint8_t>(UdsService::EcuReset), data);
}

bool UdsClient::testerPresent(bool suppressPositiveResponse)
{
    std::vector<uint8_t> data;
    uint8_t subFunc = 0x00;
    if (suppressPositiveResponse) {
        subFunc |= 0x80;
    }
    data.push_back(subFunc);
    return sendRequest(static_cast<uint8_t>(UdsService::TesterPresent), data);
}

bool UdsClient::securityAccessRequestSeed(uint8_t level)
{
    std::vector<uint8_t> data;
    data.push_back(level); // 请求种子，level为奇数
    return sendRequest(static_cast<uint8_t>(UdsService::SecurityAccess), data);
}

bool UdsClient::securityAccessSendKey(uint8_t level, const std::vector<uint8_t>& key)
{
    std::vector<uint8_t> data;
    data.push_back(level); // 发送密钥，level为偶数
    data.insert(data.end(), key.begin(), key.end());
    return sendRequest(static_cast<uint8_t>(UdsService::SecurityAccess), data);
}

bool UdsClient::startSecurityAccess(uint8_t level, KeyCalculator keyCalculator, SecurityAccessCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_securityState != SecurityAccessState::Idle && m_securityState != SecurityAccessState::Unlocked) {
        return false;  // 正在进行中
    }
    if (!keyCalculator) {
        return false;
    }

    m_keyCalculator = keyCalculator;
    m_securityCallback = callback;
    m_securityLevel = level;
    m_securityState = SecurityAccessState::WaitingForSeed;

    // 请求种子（level为奇数）
    std::vector<uint8_t> data;
    data.push_back(level);
    return sendRequest(static_cast<uint8_t>(UdsService::SecurityAccess), data);
}

SecurityAccessState UdsClient::securityAccessState() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_securityState;
}

uint8_t UdsClient::unlockedLevel() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_unlockedLevel;
}

bool UdsClient::isSecurityUnlocked(uint8_t level) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (level == 0) {
        return m_unlockedLevel != 0;
    }
    return m_unlockedLevel == level || m_unlockedLevel == level + 1;
}

bool UdsClient::sendRequest(uint8_t serviceId, const std::vector<uint8_t>& data)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // UDS半双工：如果有pending请求，拒绝新请求
    if (m_pendingRequest.valid) {
        return false;
    }

    std::vector<uint8_t> request;
    request.push_back(serviceId);
    request.insert(request.end(), data.begin(), data.end());

    bool success = m_isoTp.sendData(request);
    if (success) {
        // 记录pending请求，用于超时检测和响应匹配
        m_pendingRequest.serviceId = serviceId;
        m_pendingRequest.sendTimeUs = CanFrame::currentTimestampUs();
        m_pendingRequest.valid = true;
    }
    return success;
}

void UdsClient::reset()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_isoTp.reset();
    m_currentSession = UdsSessionType::DefaultSession;
    m_pendingRequest.valid = false;
    m_securityState = SecurityAccessState::Idle;
    m_securityLevel = 0;
    m_unlockedLevel = 0;
    m_keyCalculator = nullptr;
    m_securityCallback = nullptr;
}

UdsSessionType UdsClient::currentSession() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_currentSession;
}

void UdsClient::onIsoTpReceived(const std::vector<uint8_t>& data)
{
    parseResponse(data);
}

void UdsClient::parseResponse(const std::vector<uint8_t>& data)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (data.empty()) return;

    UdsResponse response;

    if (data[0] == 0x7F) {
        // 否定响应
        response.success = false;
        if (data.size() >= 3) {
            response.serviceId = data[1];
            response.nrc = data[2];
        }
    } else {
        // 肯定响应
        response.success = true;
        response.serviceId = data[0] - 0x40; // 肯定响应 = 请求ID + 0x40
        if (data.size() > 1) {
            response.data.assign(data.begin() + 1, data.end());
        }

        // 更新会话状态
        if (response.serviceId == static_cast<uint8_t>(UdsService::DiagnosticSessionControl)) {
            if (!response.data.empty()) {
                m_currentSession = static_cast<UdsSessionType>(response.data[0]);
            }
        }
    }

    // 响应匹配：检查是否与pending请求的serviceId一致
    if (m_pendingRequest.valid) {
        if (response.serviceId != m_pendingRequest.serviceId) {
            UdsErrorCallback errCb = m_udsErrorCallback;
            if (errCb) {
                errCb(UdsErrorType::ResponseMismatch, response.serviceId);
            }
        }
        m_pendingRequest.valid = false;
    }

    // 安全访问响应处理
    if (response.serviceId == static_cast<uint8_t>(UdsService::SecurityAccess)) {
        if (response.success) {
            if (m_securityState == SecurityAccessState::WaitingForSeed) {
                // 收到种子，计算密钥并发送
                if (m_keyCalculator && !response.data.empty()) {
                    std::vector<uint8_t> seed = response.data;  // data[0]是level，后面是种子
                    if (seed.size() > 1) {
                        seed.erase(seed.begin());  // 去掉level字节
                    }
                    std::vector<uint8_t> key = m_keyCalculator(seed);
                    m_securityState = SecurityAccessState::WaitingForKeyResponse;
                    // 发送密钥（level = 请求level + 1，即偶数）
                    securityAccessSendKey(m_securityLevel + 1, key);
                }
            } else if (m_securityState == SecurityAccessState::WaitingForKeyResponse) {
                // 密钥发送成功，已解锁
                m_securityState = SecurityAccessState::Unlocked;
                m_unlockedLevel = m_securityLevel;
                SecurityAccessCallback cb = m_securityCallback;
                if (cb) {
                    cb(true, m_unlockedLevel);
                }
                m_keyCalculator = nullptr;
                m_securityCallback = nullptr;
            }
        } else {
            // 否定响应，安全访问失败
            SecurityAccessState oldState = m_securityState;
            m_securityState = SecurityAccessState::Idle;
            SecurityAccessCallback cb = m_securityCallback;
            if (cb && oldState != SecurityAccessState::Idle) {
                cb(false, 0);
            }
            m_keyCalculator = nullptr;
            m_securityCallback = nullptr;
        }
    }

    if (m_responseCallback) {
        m_responseCallback(response);
    }
}
