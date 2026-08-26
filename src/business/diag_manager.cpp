#include "diag_manager.h"

DiagManager::DiagManager(QObject* parent)
    : QObject(parent)
    , m_udsClient(std::make_unique<UdsClient>())
    , m_timeoutTimer(new QTimer(this))
    , m_testerPresentTimer(new QTimer(this))
    , m_testerPresentEnabled(false)
    , m_txId(0x7E0)
    , m_rxId(0x7E8)
{
    m_udsClient->setResponseCallback([this](const UdsResponse& response) {
        QMetaObject::invokeMethod(this, [this, response]() {
            onUdsResponse(response);
        }, Qt::QueuedConnection);
    });

    // ISO-TP错误回调，跨线程用QueuedConnection
    m_udsClient->setErrorCallback([this](IsoTpErrorType error) {
        QMetaObject::invokeMethod(this, [this, error]() {
            emit timeoutOccurred(static_cast<int>(error));
        }, Qt::QueuedConnection);
    });

    // UDS层错误回调（请求超时、响应不匹配）
    m_udsClient->setUdsErrorCallback([this](UdsErrorType error, uint8_t serviceId) {
        QMetaObject::invokeMethod(this, [this, error, serviceId]() {
            emit udsErrorOccurred(static_cast<int>(error), serviceId);
        }, Qt::QueuedConnection);
    });

    // 每100ms检查一次超时
    connect(m_timeoutTimer, &QTimer::timeout, this, &DiagManager::onCheckTimeout);
    m_timeoutTimer->start(100);

    // TesterPresent保活定时器，默认3秒
    connect(m_testerPresentTimer, &QTimer::timeout, this, &DiagManager::onTesterPresentTimer);
    m_testerPresentTimer->setInterval(3000);
}

DiagManager::~DiagManager() = default;

void DiagManager::setTxId(uint32_t id)
{
    m_txId = id;
    m_udsClient->setTxId(id);
}

void DiagManager::setRxId(uint32_t id)
{
    m_rxId = id;
    m_udsClient->setRxId(id);
}

void DiagManager::setExtendedFrame(bool extended)
{
    m_udsClient->setExtendedFrame(extended);
}

uint32_t DiagManager::txId() const
{
    return m_txId;
}

uint32_t DiagManager::rxId() const
{
    return m_rxId;
}

void DiagManager::handleCanFrame(const CanFrame& frame)
{
    m_udsClient->handleReceivedFrame(frame);
}

void DiagManager::setSendFrameCallback(std::function<void(const CanFrame&)> callback)
{
    m_udsClient->setSendFrameCallback(callback);
}

bool DiagManager::diagnosticSessionControl(uint8_t sessionType)
{
    return m_udsClient->diagnosticSessionControl(static_cast<UdsSessionType>(sessionType));
}

bool DiagManager::readDataByIdentifier(uint16_t did)
{
    return m_udsClient->readDataByIdentifier(did);
}

bool DiagManager::writeDataByIdentifier(uint16_t did, const QByteArray& data)
{
    std::vector<uint8_t> vec(data.begin(), data.end());
    return m_udsClient->writeDataByIdentifier(did, vec);
}

bool DiagManager::routineControl(uint8_t controlType, uint16_t rid, const QByteArray& params)
{
    std::vector<uint8_t> vec(params.begin(), params.end());
    return m_udsClient->routineControl(static_cast<RoutineControlType>(controlType), rid, vec);
}

bool DiagManager::ecuReset(uint8_t resetType)
{
    return m_udsClient->ecuReset(resetType);
}

bool DiagManager::testerPresent(bool suppressPositiveResponse)
{
    return m_udsClient->testerPresent(suppressPositiveResponse);
}

UdsSessionType DiagManager::currentSession() const
{
    return m_udsClient->currentSession();
}

void DiagManager::onUdsResponse(const UdsResponse& response)
{
    QByteArray dataArray;
    if (response.success) {
        dataArray = QByteArray(reinterpret_cast<const char*>(response.data.data()),
                               static_cast<int>(response.data.size()));
    }

    emit responseReceived(response.success, response.serviceId, dataArray, response.nrc);

    if (response.success && response.serviceId == static_cast<uint8_t>(UdsService::DiagnosticSessionControl)) {
        if (!response.data.empty()) {
            emit sessionChanged(response.data[0]);
        }
    }
}

void DiagManager::onCheckTimeout()
{
    m_udsClient->checkTimeout();
}

void DiagManager::setTesterPresentEnabled(bool enabled)
{
    m_testerPresentEnabled = enabled;
    if (enabled) {
        m_testerPresentTimer->start();
    } else {
        m_testerPresentTimer->stop();
    }
}

bool DiagManager::isTesterPresentEnabled() const
{
    return m_testerPresentEnabled;
}

void DiagManager::setTesterPresentIntervalMs(int intervalMs)
{
    m_testerPresentTimer->setInterval(intervalMs);
}

void DiagManager::onTesterPresentTimer()
{
    // 只有在非默认会话且没有pending请求时才发送保活
    if (m_udsClient->currentSession() != UdsSessionType::DefaultSession
        && !m_udsClient->hasPendingRequest()) {
        m_udsClient->testerPresent(true);  // suppressPositiveResponse=true
    }
}
