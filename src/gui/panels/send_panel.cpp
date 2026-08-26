#include "send_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QRegularExpression>
#include <QMessageBox>

SendPanel::SendPanel(QWidget* parent)
    : QDockWidget("报文发送", parent)
    , m_sendCount(0)
{
    setupUi();
    m_cyclicTimer = new QTimer(this);
    connect(m_cyclicTimer, &QTimer::timeout, this, &SendPanel::onCyclicTimer);
}

SendPanel::~SendPanel() = default;

void SendPanel::setupUi()
{
    auto* widget = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(widget);

    // CAN ID
    auto* idLayout = new QHBoxLayout();
    idLayout->addWidget(new QLabel("CAN ID:"));
    m_idEdit = new QLineEdit("7E0", this);
    m_idEdit->setMaximumWidth(100);
    idLayout->addWidget(m_idEdit);
    m_extendedCheck = new QCheckBox("扩展帧", this);
    idLayout->addWidget(m_extendedCheck);
    idLayout->addStretch();
    mainLayout->addLayout(idLayout);

    // 数据
    auto* dataLayout = new QHBoxLayout();
    dataLayout->addWidget(new QLabel("数据(Hex):"));
    m_dataEdit = new QLineEdit("01 02 03 04 05 06 07 08", this);
    dataLayout->addWidget(m_dataEdit);
    mainLayout->addLayout(dataLayout);

    // 发送按钮
    m_sendBtn = new QPushButton("发送", this);
    connect(m_sendBtn, &QPushButton::clicked, this, &SendPanel::onSendClicked);
    mainLayout->addWidget(m_sendBtn);

    // 循环发送
    auto* cyclicGroup = new QGroupBox("循环发送", this);
    auto* cyclicLayout = new QHBoxLayout(cyclicGroup);
    m_cyclicCheck = new QCheckBox("启用", this);
    connect(m_cyclicCheck, &QCheckBox::toggled, this, &SendPanel::onCyclicToggled);
    cyclicLayout->addWidget(m_cyclicCheck);
    cyclicLayout->addWidget(new QLabel("周期(ms):"));
    m_periodSpin = new QSpinBox(this);
    m_periodSpin->setRange(1, 60000);
    m_periodSpin->setValue(100);
    cyclicLayout->addWidget(m_periodSpin);
    mainLayout->addWidget(cyclicGroup);

    // 计数
    m_countLabel = new QLabel("已发送: 0 帧", this);
    mainLayout->addWidget(m_countLabel);

    mainLayout->addStretch();
    setWidget(widget);
}

CanFrame SendPanel::buildFrame() const
{
    CanFrame frame;
    bool ok;
    frame.id = static_cast<uint32_t>(m_idEdit->text().toUInt(&ok, 16));
    if (!ok) frame.id = 0;
    frame.extended = m_extendedCheck->isChecked();
    parseHexData(m_dataEdit->text(), frame.data);
    frame.dlc = static_cast<uint8_t>(frame.data.size());
    frame.timestamp_us = CanFrame::currentTimestampUs();
    return frame;
}

bool SendPanel::parseHexData(const QString& text, std::vector<uint8_t>& data) const
{
    data.clear();
    QStringList bytes = text.trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (const QString& b : bytes) {
        bool ok;
        uint32_t val = b.toUInt(&ok, 16);
        if (ok && val <= 0xFF) {
            data.push_back(static_cast<uint8_t>(val));
        }
        if (data.size() >= 8) break;
    }
    return !data.empty();
}

void SendPanel::onSendClicked()
{
    if (m_dataEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "警告", "数据不能为空");
        return;
    }
    CanFrame frame = buildFrame();
    emit sendFrameRequested(frame);
    m_sendCount++;
    m_countLabel->setText(QString("已发送: %1 帧").arg(m_sendCount));
}

void SendPanel::onCyclicToggled(bool checked)
{
    if (checked) {
        m_cyclicTimer->start(m_periodSpin->value());
        m_sendBtn->setEnabled(false);
    } else {
        m_cyclicTimer->stop();
        m_sendBtn->setEnabled(true);
    }
}

void SendPanel::onCyclicTimer()
{
    CanFrame frame = buildFrame();
    emit sendFrameRequested(frame);
    m_sendCount++;
    m_countLabel->setText(QString("已发送: %1 帧").arg(m_sendCount));
}
