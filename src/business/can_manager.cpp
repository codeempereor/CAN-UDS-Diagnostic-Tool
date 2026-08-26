#include "can_manager.h"
#include "hal/virtual_can_adapter.h"

#ifdef HAS_SOCKETCAN
#include "hal/socket_can_adapter.h"
#endif

CanManager::CanManager(QObject* parent)
    : QObject(parent)
    , m_adapterType(AdapterType::Virtual)
    , m_running(false)
{
}

CanManager::~CanManager()
{
    stopAdapter();
}

bool CanManager::startAdapter(AdapterType type, const QString& interface, uint32_t bitrate)
{
    if (m_running) {
        stopAdapter();
    }

    m_adapterType = type;

    switch (type) {
    case AdapterType::Virtual:
        m_adapter = std::make_unique<VirtualCanAdapter>();
        break;
    case AdapterType::SocketCAN:
#ifdef HAS_SOCKETCAN
        m_adapter = std::make_unique<SocketCanAdapter>();
#else
        emit errorOccurred("SocketCAN not supported on this platform");
        return false;
#endif
        break;
    }

    m_adapter->setReceiveCallback([this](const CanFrame& frame) {
        QMetaObject::invokeMethod(this, [this, frame]() {
            onFrameReceived(frame);
        }, Qt::QueuedConnection);
    });

    m_adapter->setErrorCallback([this](const std::string& error) {
        QMetaObject::invokeMethod(this, [this, error]() {
            onAdapterError(error);
        }, Qt::QueuedConnection);
    });

    m_adapter->setStateChangedCallback([this](CanAdapterState state) {
        QMetaObject::invokeMethod(this, [this, state]() {
            onAdapterStateChanged(state);
        }, Qt::QueuedConnection);
    });

    m_stats.reset();
    bool success = m_adapter->open(interface.toStdString(), bitrate);

    if (success) {
        m_running = true;
        emit adapterStateChanged(true);
    }

    return success;
}

void CanManager::stopAdapter()
{
    if (m_adapter) {
        m_adapter->close();
        m_adapter.reset();
    }
    m_running = false;
    emit adapterStateChanged(false);
}

bool CanManager::isRunning() const
{
    return m_running;
}

bool CanManager::sendFrame(const CanFrame& frame)
{
    if (!m_running || !m_adapter) return false;
    return m_adapter->sendFrame(frame);
}

CanFilter& CanManager::filter()
{
    return m_filter;
}

const CanStats& CanManager::stats() const
{
    return m_stats;
}

CanManager::AdapterType CanManager::currentAdapterType() const
{
    return m_adapterType;
}

QString CanManager::adapterName() const
{
    if (m_adapter) {
        return QString::fromStdString(m_adapter->adapterName());
    }
    return QString();
}

void CanManager::onFrameReceived(const CanFrame& frame)
{
    emit frameReceived(frame);
    m_stats.update(frame);

    if (m_filter.pass(frame)) {
        emit filteredFrameReceived(frame);
    }
}

void CanManager::onAdapterError(const std::string& error)
{
    emit errorOccurred(QString::fromStdString(error));
}

void CanManager::onAdapterStateChanged(CanAdapterState state)
{
    bool running = (state == CanAdapterState::Open);
    if (m_running != running) {
        m_running = running;
        emit adapterStateChanged(running);
    }
}
