#ifndef SIGNAL_MANAGER_H
#define SIGNAL_MANAGER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QVector>
#include <QTimer>
#include <QColor>
#include <deque>

#include "can/can_frame.h"

struct SignalData {
    QString name;
    uint32_t canId = 0;
    QString unit;
    std::deque<double> values;
    std::deque<uint64_t> timestamps;
    double minValue = 0;
    double maxValue = 0;
    bool visible = true;
    QColor color;

    static constexpr size_t MAX_POINTS = 10000;
};

class SignalManager : public QObject {
    Q_OBJECT

public:
    explicit SignalManager(QObject* parent = nullptr);
    ~SignalManager() override;

    void addSignal(const QString& name, uint32_t canId);
    void removeSignal(const QString& name);
    void clearSignals();

    void setSignalVisible(const QString& name, bool visible);
    bool isSignalVisible(const QString& name) const;
    void setSignalUnit(const QString& name, const QString& unit);

    const SignalData* signalData(const QString& name) const;
    QStringList signalNames() const;
    size_t signalCount() const;

    void setMaxPoints(size_t count);

public slots:
    void onSignalValueReceived(const QString& signalName, double value, uint64_t timestamp);

signals:
    void signalAdded(const QString& name);
    void signalRemoved(const QString& name);
    void dataUpdated();

private:
    QMap<QString, SignalData> m_signals;
    size_t m_maxPoints;
    QTimer* m_updateTimer;
};

#endif // SIGNAL_MANAGER_H
