#ifndef DIAG_PANEL_H
#define DIAG_PANEL_H

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QLabel>

class DiagPanel : public QWidget {
    Q_OBJECT

public:
    explicit DiagPanel(QWidget* parent = nullptr);
    ~DiagPanel() override;

    void addResponse(bool success, uint8_t serviceId, const QByteArray& data, uint8_t nrc);

signals:
    void sendDiagnosticSession(uint8_t sessionType);
    void sendReadDid(uint16_t did);
    void sendWriteDid(uint16_t did, const QByteArray& data);
    void sendRoutineControl(uint8_t controlType, uint16_t rid, const QByteArray& params);
    void sendEcuReset();
    void sendTesterPresent();
    void txIdChanged(uint32_t id);
    void rxIdChanged(uint32_t id);

private slots:
    void onSessionControlClicked();
    void onReadDidClicked();
    void onWriteDidClicked();
    void onRoutineControlClicked();
    void onEcuResetClicked();
    void onTesterPresentClicked();
    void onTxIdChanged(const QString& text);
    void onRxIdChanged(const QString& text);

private:
    void setupUi();
    QString serviceName(uint8_t serviceId);
    QString toHexString(const QByteArray& data);

    // 配置区
    QLineEdit* m_txIdEdit;
    QLineEdit* m_rxIdEdit;

    // 标签页
    QTabWidget* m_tabWidget;

    // 会话控制页
    QComboBox* m_sessionCombo;
    QPushButton* m_sessionBtn;

    // 读DID页
    QLineEdit* m_readDidEdit;
    QPushButton* m_readDidBtn;

    // 写DID页
    QLineEdit* m_writeDidEdit;
    QLineEdit* m_writeDataEdit;
    QPushButton* m_writeDidBtn;

    // 例程控制页
    QComboBox* m_routineTypeCombo;
    QLineEdit* m_routineIdEdit;
    QLineEdit* m_routineParamsEdit;
    QPushButton* m_routineBtn;

    // 其他功能
    QPushButton* m_ecuResetBtn;
    QPushButton* m_testerPresentBtn;

    // 响应日志
    QTextEdit* m_logEdit;
};

#endif // DIAG_PANEL_H
