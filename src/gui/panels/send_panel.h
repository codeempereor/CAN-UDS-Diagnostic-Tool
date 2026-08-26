#ifndef SEND_PANEL_H
#define SEND_PANEL_H

#include <QDockWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QLabel>

#include "can/can_frame.h"

class SendPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit SendPanel(QWidget* parent = nullptr);
    ~SendPanel() override;

signals:
    void sendFrameRequested(const CanFrame& frame);

private slots:
    void onSendClicked();
    void onCyclicToggled(bool checked);
    void onCyclicTimer();

private:
    void setupUi();
    CanFrame buildFrame() const;
    bool parseHexData(const QString& text, std::vector<uint8_t>& data) const;

    QLineEdit* m_idEdit;
    QCheckBox* m_extendedCheck;
    QLineEdit* m_dataEdit;
    QPushButton* m_sendBtn;
    QCheckBox* m_cyclicCheck;
    QSpinBox* m_periodSpin;
    QLabel* m_countLabel;
    QTimer* m_cyclicTimer;
    uint64_t m_sendCount;
};

#endif // SEND_PANEL_H
