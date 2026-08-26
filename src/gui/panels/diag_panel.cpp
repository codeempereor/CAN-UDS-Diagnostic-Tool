#include "diag_panel.h"
#include <QDateTime>
#include <QMessageBox>
#include <QRegularExpression>

DiagPanel::DiagPanel(QWidget* parent)
    : QWidget(parent)
    , m_txIdEdit(nullptr)
    , m_rxIdEdit(nullptr)
    , m_tabWidget(nullptr)
    , m_sessionCombo(nullptr)
    , m_sessionBtn(nullptr)
    , m_readDidEdit(nullptr)
    , m_readDidBtn(nullptr)
    , m_writeDidEdit(nullptr)
    , m_writeDataEdit(nullptr)
    , m_writeDidBtn(nullptr)
    , m_routineTypeCombo(nullptr)
    , m_routineIdEdit(nullptr)
    , m_routineParamsEdit(nullptr)
    , m_routineBtn(nullptr)
    , m_ecuResetBtn(nullptr)
    , m_testerPresentBtn(nullptr)
    , m_logEdit(nullptr)
{
    setupUi();
}

DiagPanel::~DiagPanel() = default;

void DiagPanel::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    // 配置区
    QGroupBox* configGroup = new QGroupBox("诊断配置", this);
    QHBoxLayout* configLayout = new QHBoxLayout(configGroup);

    configLayout->addWidget(new QLabel("TX ID:"));
    m_txIdEdit = new QLineEdit("0x7E0", this);
    m_txIdEdit->setMaximumWidth(80);
    configLayout->addWidget(m_txIdEdit);

    configLayout->addWidget(new QLabel("RX ID:"));
    m_rxIdEdit = new QLineEdit("0x7E8", this);
    m_rxIdEdit->setMaximumWidth(80);
    configLayout->addWidget(m_rxIdEdit);

    configLayout->addStretch();

    mainLayout->addWidget(configGroup);

    // 功能标签页
    m_tabWidget = new QTabWidget(this);

    // 会话控制页
    QWidget* sessionPage = new QWidget();
    QVBoxLayout* sessionLayout = new QVBoxLayout(sessionPage);

    QHBoxLayout* sessionRow = new QHBoxLayout();
    sessionRow->addWidget(new QLabel("会话类型:"));
    m_sessionCombo = new QComboBox(this);
    m_sessionCombo->addItem("默认会话 (0x01)", 0x01);
    m_sessionCombo->addItem("编程会话 (0x02)", 0x02);
    m_sessionCombo->addItem("扩展会话 (0x03)", 0x03);
    m_sessionCombo->addItem("安全会话 (0x04)", 0x04);
    sessionRow->addWidget(m_sessionCombo, 1);
    sessionLayout->addLayout(sessionRow);

    m_sessionBtn = new QPushButton("切换会话", this);
    sessionLayout->addWidget(m_sessionBtn);
    sessionLayout->addStretch();

    m_tabWidget->addTab(sessionPage, "会话控制");

    // 读DID页
    QWidget* readPage = new QWidget();
    QVBoxLayout* readLayout = new QVBoxLayout(readPage);

    QHBoxLayout* readRow = new QHBoxLayout();
    readRow->addWidget(new QLabel("DID:"));
    m_readDidEdit = new QLineEdit("0xF190", this);
    m_readDidEdit->setPlaceholderText("如: 0xF190");
    readRow->addWidget(m_readDidEdit, 1);
    readLayout->addLayout(readRow);

    m_readDidBtn = new QPushButton("读取数据", this);
    readLayout->addWidget(m_readDidBtn);
    readLayout->addStretch();

    m_tabWidget->addTab(readPage, "读DID");

    // 写DID页
    QWidget* writePage = new QWidget();
    QVBoxLayout* writeLayout = new QVBoxLayout(writePage);

    QHBoxLayout* writeDidRow = new QHBoxLayout();
    writeDidRow->addWidget(new QLabel("DID:"));
    m_writeDidEdit = new QLineEdit("0x1234", this);
    writeDidRow->addWidget(m_writeDidEdit, 1);
    writeLayout->addLayout(writeDidRow);

    QHBoxLayout* writeDataRow = new QHBoxLayout();
    writeDataRow->addWidget(new QLabel("数据:"));
    m_writeDataEdit = new QLineEdit("01 02 03 04", this);
    m_writeDataEdit->setPlaceholderText("十六进制，空格分隔");
    writeDataRow->addWidget(m_writeDataEdit, 1);
    writeLayout->addLayout(writeDataRow);

    m_writeDidBtn = new QPushButton("写入数据", this);
    writeLayout->addWidget(m_writeDidBtn);
    writeLayout->addStretch();

    m_tabWidget->addTab(writePage, "写DID");

    // 例程控制页
    QWidget* routinePage = new QWidget();
    QVBoxLayout* routineLayout = new QVBoxLayout(routinePage);

    QHBoxLayout* routineTypeRow = new QHBoxLayout();
    routineTypeRow->addWidget(new QLabel("控制类型:"));
    m_routineTypeCombo = new QComboBox(this);
    m_routineTypeCombo->addItem("启动例程 (0x01)", 0x01);
    m_routineTypeCombo->addItem("停止例程 (0x02)", 0x02);
    m_routineTypeCombo->addItem("查询结果 (0x03)", 0x03);
    routineTypeRow->addWidget(m_routineTypeCombo, 1);
    routineLayout->addLayout(routineTypeRow);

    QHBoxLayout* routineIdRow = new QHBoxLayout();
    routineIdRow->addWidget(new QLabel("例程ID:"));
    m_routineIdEdit = new QLineEdit("0x0202", this);
    routineIdRow->addWidget(m_routineIdEdit, 1);
    routineLayout->addLayout(routineIdRow);

    QHBoxLayout* routineParamsRow = new QHBoxLayout();
    routineParamsRow->addWidget(new QLabel("参数:"));
    m_routineParamsEdit = new QLineEdit("", this);
    m_routineParamsEdit->setPlaceholderText("可选，十六进制");
    routineParamsRow->addWidget(m_routineParamsEdit, 1);
    routineLayout->addLayout(routineParamsRow);

    m_routineBtn = new QPushButton("执行例程控制", this);
    routineLayout->addWidget(m_routineBtn);
    routineLayout->addStretch();

    m_tabWidget->addTab(routinePage, "例程控制");

    // 其他功能页
    QWidget* otherPage = new QWidget();
    QVBoxLayout* otherLayout = new QVBoxLayout(otherPage);

    m_ecuResetBtn = new QPushButton("ECU复位 (0x11)", this);
    otherLayout->addWidget(m_ecuResetBtn);

    m_testerPresentBtn = new QPushButton("TesterPresent (0x3E)", this);
    otherLayout->addWidget(m_testerPresentBtn);

    otherLayout->addStretch();

    m_tabWidget->addTab(otherPage, "其他功能");

    mainLayout->addWidget(m_tabWidget);

    // 响应日志
    QGroupBox* logGroup = new QGroupBox("诊断日志", this);
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);
    m_logEdit = new QTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumHeight(200);
    logLayout->addWidget(m_logEdit);
    mainLayout->addWidget(logGroup);

    // 信号连接
    connect(m_sessionBtn, &QPushButton::clicked, this, &DiagPanel::onSessionControlClicked);
    connect(m_readDidBtn, &QPushButton::clicked, this, &DiagPanel::onReadDidClicked);
    connect(m_writeDidBtn, &QPushButton::clicked, this, &DiagPanel::onWriteDidClicked);
    connect(m_routineBtn, &QPushButton::clicked, this, &DiagPanel::onRoutineControlClicked);
    connect(m_ecuResetBtn, &QPushButton::clicked, this, &DiagPanel::onEcuResetClicked);
    connect(m_testerPresentBtn, &QPushButton::clicked, this, &DiagPanel::onTesterPresentClicked);
    connect(m_txIdEdit, &QLineEdit::editingFinished, this, [this]() { onTxIdChanged(m_txIdEdit->text()); });
    connect(m_rxIdEdit, &QLineEdit::editingFinished, this, [this]() { onRxIdChanged(m_rxIdEdit->text()); });
}

void DiagPanel::addResponse(bool success, uint8_t serviceId, const QByteArray& data, uint8_t nrc)
{
    QString time = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    QString line;

    if (success) {
        line = QString("[%1] <font color='green'>肯定响应</font> 服务: 0x%2 (%3) 数据: %4")
                   .arg(time)
                   .arg(serviceId, 2, 16, QChar('0')).toUpper()
                   .arg(serviceName(serviceId))
                   .arg(toHexString(data));
    } else {
        line = QString("[%1] <font color='red'>否定响应</font> 服务: 0x%2 (%3) NRC: 0x%4")
                   .arg(time)
                   .arg(serviceId, 2, 16, QChar('0')).toUpper()
                   .arg(serviceName(serviceId))
                   .arg(nrc, 2, 16, QChar('0')).toUpper();
    }

    m_logEdit->append(line);
}

void DiagPanel::onSessionControlClicked()
{
    uint8_t session = static_cast<uint8_t>(m_sessionCombo->currentData().toUInt());
    emit sendDiagnosticSession(session);

    QString time = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    m_logEdit->append(QString("[%1] <font color='blue'>发送</font> 会话控制: 0x%2")
        .arg(time).arg(session, 2, 16, QChar('0')).toUpper());
}

void DiagPanel::onReadDidClicked()
{
    bool ok;
    uint16_t did = static_cast<uint16_t>(m_readDidEdit->text().toUInt(&ok, 0));
    if (!ok) {
        QMessageBox::warning(this, "错误", "请输入有效的DID值");
        return;
    }
    emit sendReadDid(did);

    QString time = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    m_logEdit->append(QString("[%1] <font color='blue'>发送</font> 读DID: 0x%2")
        .arg(time).arg(did, 4, 16, QChar('0')).toUpper());
}

void DiagPanel::onWriteDidClicked()
{
    bool ok;
    uint16_t did = static_cast<uint16_t>(m_writeDidEdit->text().toUInt(&ok, 0));
    if (!ok) {
        QMessageBox::warning(this, "错误", "请输入有效的DID值");
        return;
    }

    // 解析数据
    QByteArray data;
    QStringList bytes = m_writeDataEdit->text().trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (const QString& b : bytes) {
        bool byteOk;
        uint8_t val = static_cast<uint8_t>(b.toUInt(&byteOk, 16));
        if (byteOk) {
            data.append(val);
        }
    }

    emit sendWriteDid(did, data);

    QString time = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    m_logEdit->append(QString("[%1] <font color='blue'>发送</font> 写DID: 0x%2 数据: %3")
        .arg(time).arg(did, 4, 16, QChar('0')).toUpper().arg(toHexString(data)));
}

void DiagPanel::onRoutineControlClicked()
{
    uint8_t type = static_cast<uint8_t>(m_routineTypeCombo->currentData().toUInt());

    bool ok;
    uint16_t rid = static_cast<uint16_t>(m_routineIdEdit->text().toUInt(&ok, 0));
    if (!ok) {
        QMessageBox::warning(this, "错误", "请输入有效的例程ID");
        return;
    }

    QByteArray params;
    QStringList bytes = m_routineParamsEdit->text().trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (const QString& b : bytes) {
        bool byteOk;
        uint8_t val = static_cast<uint8_t>(b.toUInt(&byteOk, 16));
        if (byteOk) {
            params.append(val);
        }
    }

    emit sendRoutineControl(type, rid, params);

    QString time = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    QString typeName = (type == 0x01) ? "启动" : (type == 0x02) ? "停止" : "查询";
    m_logEdit->append(QString("[%1] <font color='blue'>发送</font> 例程控制[%2]: 0x%3")
        .arg(time).arg(typeName).arg(rid, 4, 16, QChar('0')).toUpper());
}

void DiagPanel::onEcuResetClicked()
{
    emit sendEcuReset();

    QString time = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    m_logEdit->append(QString("[%1] <font color='blue'>发送</font> ECU复位").arg(time));
}

void DiagPanel::onTesterPresentClicked()
{
    emit sendTesterPresent();

    QString time = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    m_logEdit->append(QString("[%1] <font color='blue'>发送</font> TesterPresent").arg(time));
}

void DiagPanel::onTxIdChanged(const QString& text)
{
    bool ok;
    uint32_t id = text.toUInt(&ok, 0);
    if (ok) {
        emit txIdChanged(id);
    }
}

void DiagPanel::onRxIdChanged(const QString& text)
{
    bool ok;
    uint32_t id = text.toUInt(&ok, 0);
    if (ok) {
        emit rxIdChanged(id);
    }
}

QString DiagPanel::serviceName(uint8_t serviceId)
{
    switch (serviceId) {
    case 0x10: return "会话控制";
    case 0x11: return "ECU复位";
    case 0x22: return "读数据标识符";
    case 0x2E: return "写数据标识符";
    case 0x27: return "安全访问";
    case 0x31: return "例程控制";
    case 0x34: return "请求下载";
    case 0x36: return "数据传输";
    case 0x37: return "传输退出";
    case 0x3E: return "TesterPresent";
    default: return "未知服务";
    }
}

QString DiagPanel::toHexString(const QByteArray& data)
{
    QStringList bytes;
    for (int i = 0; i < data.size(); ++i) {
        bytes.append(QString("%1").arg(static_cast<uint8_t>(data[i]), 2, 16, QChar('0')).toUpper());
    }
    return bytes.join(" ");
}
