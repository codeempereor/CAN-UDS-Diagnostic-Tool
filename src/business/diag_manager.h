#ifndef DIAG_MANAGER_H
#define DIAG_MANAGER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <memory>

#include "uds/uds_client.h"
#include "can/can_frame.h"

class DiagManager : public QObject {
    Q_OBJECT

public:
    explicit DiagManager(QObject* parent = nullptr);
    ~DiagManager() override;

    void setTxId(uint32_t id);
    void setRxId(uint32_t id);
    void setExtendedFrame(bool extended);

    uint32_t txId() const;
    uint32_t rxId() const;

    // 传入CAN帧给UDS层处理
    void handleCanFrame(const CanFrame& frame);

    // 设置发送CAN帧的回调
    void setSendFrameCallback(std::function<void(const CanFrame&)> callback);

    // 诊断服务
    Q_INVOKABLE bool diagnosticSessionControl(uint8_t sessionType);
    Q_INVOKABLE bool readDataByIdentifier(uint16_t did);
    Q_INVOKABLE bool writeDataByIdentifier(uint16_t did, const QByteArray& data);
    Q_INVOKABLE bool routineControl(uint8_t controlType, uint16_t rid, const QByteArray& params = QByteArray());
    Q_INVOKABLE bool ecuReset(uint8_t resetType = 0x01);
    Q_INVOKABLE bool testerPresent(bool suppressPositiveResponse = false);

    UdsSessionType currentSession() const;

    // TesterPresent自动保活
    void setTesterPresentEnabled(bool enabled);
    bool isTesterPresentEnabled() const;
    void setTesterPresentIntervalMs(int intervalMs);

signals:
    void responseReceived(bool success, uint8_t serviceId, const QByteArray& data, uint8_t nrc);
    void sessionChanged(uint8_t sessionType);
    void timeoutOccurred(int errorType);  // ISO-TP超时/错误通知，errorType对应IsoTpErrorType
    void udsErrorOccurred(int errorType, uint8_t serviceId);  // UDS层错误，errorType对应UdsErrorType

private slots:
    void onUdsResponse(const UdsResponse& response);
    void onCheckTimeout();
    void onTesterPresentTimer();

private:
    std::unique_ptr<UdsClient> m_udsClient;
    QTimer* m_timeoutTimer;
    QTimer* m_testerPresentTimer;
    bool m_testerPresentEnabled;
    uint32_t m_txId;
    uint32_t m_rxId;
};

#endif // DIAG_MANAGER_H
