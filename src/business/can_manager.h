#ifndef CAN_MANAGER_H
#define CAN_MANAGER_H

#include <QObject>
#include <QThread>
#include <QString>
#include <memory>

#include "hal/can_adapter_base.h"
#include "can/can_filter.h"
#include "can/can_stats.h"

class CanManager : public QObject {
    Q_OBJECT

public:
    explicit CanManager(QObject* parent = nullptr);
    ~CanManager() override;

    // 适配器类型
    enum class AdapterType {
        Virtual,
        SocketCAN
    };

    bool startAdapter(AdapterType type, const QString& interface = QString(), uint32_t bitrate = 500000);
    void stopAdapter();
    bool isRunning() const;

    bool sendFrame(const CanFrame& frame);

    CanFilter& filter();
    const CanStats& stats() const;

    AdapterType currentAdapterType() const;
    QString adapterName() const;

signals:
    void frameReceived(const CanFrame& frame);
    void filteredFrameReceived(const CanFrame& frame);
    void adapterStateChanged(bool running);
    void errorOccurred(const QString& error);

private slots:
    void onFrameReceived(const CanFrame& frame);
    void onAdapterError(const std::string& error);
    void onAdapterStateChanged(CanAdapterState state);

private:
    std::unique_ptr<CanAdapterBase> m_adapter;
    AdapterType m_adapterType;
    CanFilter m_filter;
    CanStats m_stats;
    bool m_running;
};

#endif // CAN_MANAGER_H
