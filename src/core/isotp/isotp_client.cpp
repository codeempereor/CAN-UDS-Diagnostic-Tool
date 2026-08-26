#include "isotp/isotp_client.h"
#include <cstring>
#include <algorithm>

IsoTpClient::IsoTpClient()
    : m_txId(0x7E0)
    , m_rxId(0x7E8)
    , m_extended(false)
    , m_txIndex(0)
    , m_txSequence(0)
    , m_blockSize(8)
    , m_stMin(0)
    , m_remainingInBlock(0)
    , m_waitingForFlowControl(false)
    , m_rxTotalLength(0)
    , m_rxReceived(0)
    , m_rxSequence(0)
    , m_rxBlockSize(8)
    , m_rxStMin(0)
    , m_state(IsoTpState::Idle)
    , m_lastFrameTimeUs(0)
{
}

IsoTpClient::~IsoTpClient()
{
}

void IsoTpClient::setTxId(uint32_t id)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_txId = id;
}

void IsoTpClient::setRxId(uint32_t id)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_rxId = id;
}

void IsoTpClient::setExtendedFrame(bool extended)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_extended = extended;
}

void IsoTpClient::setReceiveCallback(ReceiveCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_receiveCallback = callback;
}

void IsoTpClient::setSendFrameCallback(SendFrameCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_sendFrameCallback = callback;
}

void IsoTpClient::setErrorCallback(ErrorCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_errorCallback = callback;
}

bool IsoTpClient::sendData(const std::vector<uint8_t>& data)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (data.empty()) return false;
    if (!m_sendFrameCallback) return false;

    if (data.size() <= 7) {
        return sendSingleFrame(data);
    } else {
        m_txBuffer = data;
        m_txIndex = 0;
        m_txSequence = 1;
        m_state = IsoTpState::Sending;
        m_waitingForFlowControl = true;

        // 发送首帧
        std::vector<uint8_t> firstData;
        size_t firstLen = std::min<size_t>(6, data.size());
        firstData.assign(data.begin(), data.begin() + firstLen);
        m_txIndex = firstLen;

        return sendFirstFrame(static_cast<uint32_t>(data.size()), firstData);
    }
}

void IsoTpClient::handleReceivedFrame(const CanFrame& frame)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (frame.id != m_rxId) return;
    if (frame.dlc < 1) return;

    uint8_t pci = frame.data[0];
    uint8_t frameType = (pci >> 4) & 0x0F;

    m_lastFrameTimeUs = frame.timestamp_us;

    switch (static_cast<IsoTpFrameType>(frameType)) {
    case IsoTpFrameType::SingleFrame:
        handleSingleFrame(frame);
        break;
    case IsoTpFrameType::FirstFrame:
        handleFirstFrame(frame);
        break;
    case IsoTpFrameType::ConsecutiveFrame:
        handleConsecutiveFrame(frame);
        break;
    case IsoTpFrameType::FlowControlFrame:
        handleFlowControlFrame(frame);
        break;
    }
}

void IsoTpClient::setBlockSize(uint8_t bs)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_blockSize = bs;
}

void IsoTpClient::setStMin(uint8_t st_min)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_stMin = st_min;
}

void IsoTpClient::reset()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_state = IsoTpState::Idle;
    m_txBuffer.clear();
    m_txIndex = 0;
    m_txSequence = 0;
    m_waitingForFlowControl = false;
    m_rxBuffer.clear();
    m_rxTotalLength = 0;
    m_rxReceived = 0;
    m_rxSequence = 0;
}

IsoTpState IsoTpClient::state() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_state;
}

bool IsoTpClient::checkTimeout()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_state == IsoTpState::Idle || m_lastFrameTimeUs == 0) {
        return false;
    }

    uint64_t now = CanFrame::currentTimestampUs();
    uint64_t elapsed = now - m_lastFrameTimeUs;

    if (elapsed < TIMEOUT_US) {
        return false;
    }

    // 超时了，判断是哪种超时
    IsoTpErrorType errorType;
    if (m_state == IsoTpState::Sending && m_waitingForFlowControl) {
        errorType = IsoTpErrorType::TxFlowControlTimeout;
    } else if (m_state == IsoTpState::Receiving) {
        errorType = IsoTpErrorType::RxConsecutiveTimeout;
    } else {
        return false;
    }

    // 保存回调，reset后会清空
    ErrorCallback cb = m_errorCallback;
    reset();

    if (cb) {
        cb(errorType);
    }
    return true;
}

bool IsoTpClient::sendSingleFrame(const std::vector<uint8_t>& data)
{
    CanFrame frame;
    frame.id = m_txId;
    frame.extended = m_extended;
    frame.dlc = static_cast<uint8_t>(data.size() + 1);
    frame.data.resize(frame.dlc);
    frame.data[0] = static_cast<uint8_t>(data.size());
    std::memcpy(frame.data.data() + 1, data.data(), data.size());
    frame.timestamp_us = CanFrame::currentTimestampUs();

    if (m_sendFrameCallback) {
        m_sendFrameCallback(frame);
    }
    return true;
}

bool IsoTpClient::sendFirstFrame(uint32_t totalLen, const std::vector<uint8_t>& firstData)
{
    CanFrame frame;
    frame.id = m_txId;
    frame.extended = m_extended;
    frame.dlc = 8;
    frame.data.resize(8);

    // 首帧PCI：高4位0x1，低12位长度
    frame.data[0] = 0x10 | static_cast<uint8_t>((totalLen >> 8) & 0x0F);
    frame.data[1] = static_cast<uint8_t>(totalLen & 0xFF);

    size_t copyLen = std::min<size_t>(6, firstData.size());
    std::memcpy(frame.data.data() + 2, firstData.data(), copyLen);

    frame.timestamp_us = CanFrame::currentTimestampUs();
    m_lastFrameTimeUs = frame.timestamp_us;  // 记录发送时间，用于超时检测

    if (m_sendFrameCallback) {
        m_sendFrameCallback(frame);
    }
    return true;
}

void IsoTpClient::sendConsecutiveFrames()
{
    while (m_txIndex < m_txBuffer.size() && m_remainingInBlock > 0) {
        CanFrame frame;
        frame.id = m_txId;
        frame.extended = m_extended;

        size_t remaining = m_txBuffer.size() - m_txIndex;
        size_t frameLen = std::min<size_t>(7, remaining);

        frame.dlc = static_cast<uint8_t>(frameLen + 1);
        frame.data.resize(frame.dlc);
        frame.data[0] = 0x20 | (m_txSequence & 0x0F);
        std::memcpy(frame.data.data() + 1, m_txBuffer.data() + m_txIndex, frameLen);

        frame.timestamp_us = CanFrame::currentTimestampUs();

        if (m_sendFrameCallback) {
            m_sendFrameCallback(frame);
        }

        m_txIndex += frameLen;
        m_txSequence = (m_txSequence + 1) & 0x0F;
        m_remainingInBlock--;
    }

    if (m_txIndex >= m_txBuffer.size()) {
        m_state = IsoTpState::Idle;
        m_txBuffer.clear();
    } else if (m_remainingInBlock == 0) {
        m_waitingForFlowControl = true;
    }
}

void IsoTpClient::sendFlowControl(FlowControlFlag flag, uint8_t bs, uint8_t st_min)
{
    CanFrame frame;
    frame.id = m_txId;
    frame.extended = m_extended;
    frame.dlc = 3;
    frame.data.resize(3);
    frame.data[0] = 0x30 | static_cast<uint8_t>(flag);
    frame.data[1] = bs;
    frame.data[2] = st_min;
    frame.timestamp_us = CanFrame::currentTimestampUs();

    if (m_sendFrameCallback) {
        m_sendFrameCallback(frame);
    }
}

void IsoTpClient::handleSingleFrame(const CanFrame& frame)
{
    uint8_t len = frame.data[0] & 0x0F;
    if (len == 0 || len > frame.dlc - 1) return;

    std::vector<uint8_t> data(frame.data.begin() + 1, frame.data.begin() + 1 + len);

    if (m_receiveCallback) {
        m_receiveCallback(data);
    }
}

void IsoTpClient::handleFirstFrame(const CanFrame& frame)
{
    if (frame.dlc < 2) return;

    uint32_t totalLen = ((frame.data[0] & 0x0F) << 8) | frame.data[1];
    if (totalLen == 0) return;

    m_rxBuffer.clear();
    m_rxBuffer.reserve(totalLen);
    m_rxTotalLength = totalLen;
    m_rxReceived = 0;
    m_rxSequence = 1;
    m_state = IsoTpState::Receiving;

    // 首帧中的数据
    size_t dataLen = std::min<size_t>(6, frame.dlc - 2);
    m_rxBuffer.insert(m_rxBuffer.end(), frame.data.begin() + 2, frame.data.begin() + 2 + dataLen);
    m_rxReceived += dataLen;

    // 发送流控帧
    sendFlowControl(FlowControlFlag::ContinueToSend, m_rxBlockSize, m_rxStMin);
}

void IsoTpClient::handleConsecutiveFrame(const CanFrame& frame)
{
    if (m_state != IsoTpState::Receiving) return;

    uint8_t seq = frame.data[0] & 0x0F;
    if (seq != m_rxSequence) {
        // 序列号不匹配，通知错误后重置
        ErrorCallback cb = m_errorCallback;
        reset();
        if (cb) cb(IsoTpErrorType::SequenceError);
        return;
    }

    size_t dataLen = std::min<size_t>(7, frame.dlc - 1);
    size_t remaining = m_rxTotalLength - m_rxReceived;
    size_t copyLen = std::min(dataLen, remaining);

    m_rxBuffer.insert(m_rxBuffer.end(), frame.data.begin() + 1, frame.data.begin() + 1 + copyLen);
    m_rxReceived += copyLen;
    m_rxSequence = (m_rxSequence + 1) & 0x0F;

    if (m_rxReceived >= m_rxTotalLength) {
        // 接收完成
        m_state = IsoTpState::Idle;
        if (m_receiveCallback) {
            m_receiveCallback(m_rxBuffer);
        }
        m_rxBuffer.clear();
        m_rxTotalLength = 0;
        m_rxReceived = 0;
    }
}

void IsoTpClient::handleFlowControlFrame(const CanFrame& frame)
{
    if (m_state != IsoTpState::Sending || !m_waitingForFlowControl) return;
    if (frame.dlc < 3) return;

    uint8_t flag = frame.data[0] & 0x0F;
    uint8_t bs = frame.data[1];
    uint8_t stMin = frame.data[2];

    if (flag == static_cast<uint8_t>(FlowControlFlag::Overflow)) {
        ErrorCallback cb = m_errorCallback;
        reset();
        if (cb) cb(IsoTpErrorType::Overflow);
        return;
    }

    if (flag == static_cast<uint8_t>(FlowControlFlag::Wait)) {
        // 等待，保持状态
        return;
    }

    // ContinueToSend
    m_blockSize = bs;
    m_stMin = stMin;
    m_remainingInBlock = (bs == 0) ? 0xFF : bs;
    m_waitingForFlowControl = false;

    sendConsecutiveFrames();
}
