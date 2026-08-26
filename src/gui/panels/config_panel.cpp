#include "config_panel.h"
#include <QHBoxLayout>

ConfigPanel::ConfigPanel(QWidget* parent)
    : QWidget(parent)
    , m_hardwareGroup(nullptr)
    , m_adapterCombo(nullptr)
    , m_interfaceEdit(nullptr)
    , m_bitrateCombo(nullptr)
    , m_startBtn(nullptr)
    , m_stopBtn(nullptr)
    , m_filterGroup(nullptr)
    , m_filterEdit(nullptr)
    , m_filterModeCombo(nullptr)
    , m_applyFilterBtn(nullptr)
    , m_started(false)
{
    setupUi();
}

ConfigPanel::~ConfigPanel() = default;

void ConfigPanel::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    // 硬件配置
    m_hardwareGroup = new QGroupBox("硬件配置", this);
    QVBoxLayout* hwLayout = new QVBoxLayout(m_hardwareGroup);

    hwLayout->addWidget(new QLabel("适配器类型:"));
    m_adapterCombo = new QComboBox(this);
    m_adapterCombo->addItem("虚拟适配器 (模拟)", static_cast<int>(CanManager::AdapterType::Virtual));
#ifdef HAS_SOCKETCAN
    m_adapterCombo->addItem("SocketCAN (Linux)", static_cast<int>(CanManager::AdapterType::SocketCAN));
#endif
    hwLayout->addWidget(m_adapterCombo);

    hwLayout->addWidget(new QLabel("接口名:"));
    m_interfaceEdit = new QLineEdit("vcan0", this);
    hwLayout->addWidget(m_interfaceEdit);

    hwLayout->addWidget(new QLabel("波特率:"));
    m_bitrateCombo = new QComboBox(this);
    m_bitrateCombo->addItem("125 kbps", 125000);
    m_bitrateCombo->addItem("250 kbps", 250000);
    m_bitrateCombo->addItem("500 kbps", 500000);
    m_bitrateCombo->addItem("1 Mbps", 1000000);
    m_bitrateCombo->setCurrentIndex(2); // 默认500k
    hwLayout->addWidget(m_bitrateCombo);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_startBtn = new QPushButton("启动", this);
    m_stopBtn = new QPushButton("停止", this);
    m_stopBtn->setEnabled(false);
    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(m_stopBtn);
    hwLayout->addLayout(btnLayout);

    mainLayout->addWidget(m_hardwareGroup);

    // 过滤配置
    m_filterGroup = new QGroupBox("ID过滤", this);
    QVBoxLayout* filterLayout = new QVBoxLayout(m_filterGroup);

    filterLayout->addWidget(new QLabel("过滤模式:"));
    m_filterModeCombo = new QComboBox(this);
    m_filterModeCombo->addItem("黑名单", 0);
    m_filterModeCombo->addItem("白名单", 1);
    filterLayout->addWidget(m_filterModeCombo);

    filterLayout->addWidget(new QLabel("过滤ID:"));
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("如: 0x123, 0x456");
    filterLayout->addWidget(m_filterEdit);

    m_applyFilterBtn = new QPushButton("应用过滤", this);
    filterLayout->addWidget(m_applyFilterBtn);

    mainLayout->addWidget(m_filterGroup);

    mainLayout->addStretch();

    // 信号连接
    connect(m_startBtn, &QPushButton::clicked, this, &ConfigPanel::onStartClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &ConfigPanel::onStopClicked);
    connect(m_adapterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConfigPanel::onAdapterTypeChanged);
    connect(m_applyFilterBtn, &QPushButton::clicked, this, &ConfigPanel::filterChanged);
}

void ConfigPanel::onStartClicked()
{
    CanManager::AdapterType type = static_cast<CanManager::AdapterType>(
        m_adapterCombo->currentData().toInt());
    QString iface = m_interfaceEdit->text();
    uint32_t bitrate = static_cast<uint32_t>(m_bitrateCombo->currentData().toUInt());

    emit startRequested(type, iface, bitrate);

    m_started = true;
    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_adapterCombo->setEnabled(false);
    m_interfaceEdit->setEnabled(false);
    m_bitrateCombo->setEnabled(false);
}

void ConfigPanel::onStopClicked()
{
    emit stopRequested();

    m_started = false;
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_adapterCombo->setEnabled(true);
    m_interfaceEdit->setEnabled(true);
    m_bitrateCombo->setEnabled(true);
}

void ConfigPanel::onAdapterTypeChanged(int index)
{
    CanManager::AdapterType type = static_cast<CanManager::AdapterType>(
        m_adapterCombo->itemData(index).toInt());

    if (type == CanManager::AdapterType::Virtual) {
        m_interfaceEdit->setText("vcan0");
        m_interfaceEdit->setEnabled(false);
    } else {
        m_interfaceEdit->setEnabled(true);
    }
}
