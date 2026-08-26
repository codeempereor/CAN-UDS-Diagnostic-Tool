#include "signal_manager.h"
#include <QRandomGenerator>

SignalManager::SignalManager(QObject* parent)
    : QObject(parent)
    , m_maxPoints(SignalData::MAX_POINTS)
{
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(50); // 50ms刷新一次
    connect(m_updateTimer, &QTimer::timeout, this, [this]() {
        emit dataUpdated();
    });
    m_updateTimer->start();
}

SignalManager::~SignalManager() = default;

void SignalManager::addSignal(const QString& name, uint32_t canId)
{
    if (m_signals.contains(name)) return;

    SignalData data;
    data.name = name;
    data.canId = canId;
    data.visible = true;

    // 随机颜色
    QColor color;
    color.setHsv(QRandomGenerator::global()->bounded(360), 200, 200);
    data.color = color;

    m_signals[name] = data;
    emit signalAdded(name);
}

void SignalManager::removeSignal(const QString& name)
{
    if (!m_signals.contains(name)) return;
    m_signals.remove(name);
    emit signalRemoved(name);
}

void SignalManager::clearSignals()
{
    QStringList names = m_signals.keys();
    m_signals.clear();
    for (const QString& name : names) {
        emit signalRemoved(name);
    }
}

void SignalManager::setSignalVisible(const QString& name, bool visible)
{
    if (!m_signals.contains(name)) return;
    m_signals[name].visible = visible;
}

bool SignalManager::isSignalVisible(const QString& name) const
{
    if (!m_signals.contains(name)) return false;
    return m_signals[name].visible;
}

void SignalManager::setSignalUnit(const QString& name, const QString& unit)
{
    if (m_signals.contains(name)) {
        m_signals[name].unit = unit;
    }
}

const SignalData* SignalManager::signalData(const QString& name) const
{
    auto it = m_signals.constFind(name);
    if (it == m_signals.constEnd()) return nullptr;
    return &it.value();
}

QStringList SignalManager::signalNames() const
{
    return m_signals.keys();
}

size_t SignalManager::signalCount() const
{
    return m_signals.size();
}

void SignalManager::setMaxPoints(size_t count)
{
    m_maxPoints = count;
}

void SignalManager::onSignalValueReceived(const QString& signalName, double value, uint64_t timestamp)
{
    if (!m_signals.contains(signalName)) return;

    SignalData& data = m_signals[signalName];
    data.values.push_back(value);
    data.timestamps.push_back(timestamp);

    // 更新最大最小值
    if (data.values.size() == 1) {
        data.minValue = value;
        data.maxValue = value;
    } else {
        if (value < data.minValue) data.minValue = value;
        if (value > data.maxValue) data.maxValue = value;
    }

    // 限制最大点数
    while (data.values.size() > m_maxPoints) {
        data.values.pop_front();
        data.timestamps.pop_front();
    }
}
