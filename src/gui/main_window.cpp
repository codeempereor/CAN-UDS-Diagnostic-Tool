#include "main_window.h"
#include "panels/trace_panel.h"
#include "panels/signal_panel.h"
#include "panels/diag_panel.h"
#include "panels/config_panel.h"
#include "panels/replay_panel.h"
#include "panels/send_panel.h"
#include "panels/signal_value_panel.h"
#include "panels/stats_panel.h"

#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QStyle>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_canManager(nullptr)
    , m_dbcManager(nullptr)
    , m_diagManager(nullptr)
    , m_logManager(nullptr)
    , m_signalManager(nullptr)
    , m_tracePanel(nullptr)
    , m_signalPanel(nullptr)
    , m_diagPanel(nullptr)
    , m_configPanel(nullptr)
    , m_replayPanel(nullptr)
    , m_actionStartStop(nullptr)
    , m_actionLoadDbc(nullptr)
    , m_actionRecord(nullptr)
    , m_actionLoadLog(nullptr)
    , m_statusLabel(nullptr)
    , m_statsLabel(nullptr)
    , m_statsTimer(nullptr)
    , m_running(false)
{
    setupUi();
    setupMenus();
    setupToolbar();
    setupStatusBar();
    setupDockWidgets();
    connectSignals();

    setWindowTitle("CAN/UDS 总线分析与诊断工具");
    resize(1600, 1000);
    setMinimumSize(1280, 720);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    m_canManager = new CanManager(this);
    m_dbcManager = new DbcManager(this);
    m_diagManager = new DiagManager(this);
    m_logManager = new LogManager(this);
    m_signalManager = new SignalManager(this);

    // 设置诊断层的发送回调
    m_diagManager->setSendFrameCallback([this](const CanFrame& frame) {
        m_canManager->sendFrame(frame);
    });

    m_statsTimer = new QTimer(this);
    m_statsTimer->setInterval(500);
}

void MainWindow::setupMenus()
{
    QMenu* fileMenu = menuBar()->addMenu("文件(&F)");

    m_actionLoadDbc = new QAction("加载DBC文件...", this);
    m_actionLoadDbc->setShortcut(QKeySequence("Ctrl+D"));
    fileMenu->addAction(m_actionLoadDbc);

    fileMenu->addSeparator();

    m_actionLoadLog = new QAction("加载日志文件...", this);
    fileMenu->addAction(m_actionLoadLog);

    m_actionRecord = new QAction("开始记录", this);
    m_actionRecord->setShortcut(QKeySequence("Ctrl+R"));
    fileMenu->addAction(m_actionRecord);

    fileMenu->addSeparator();

    QAction* exitAction = fileMenu->addAction("退出", this, &QWidget::close);
    exitAction->setShortcut(QKeySequence("Ctrl+Q"));

    QMenu* viewMenu = menuBar()->addMenu("视图(&V)");
    viewMenu->addAction("重置布局", this, [this]() {
        // 简单实现：重置所有dock widget
        for (QDockWidget* dock : findChildren<QDockWidget*>()) {
            dock->setVisible(true);
            dock->setFloating(false);
        }
    });

    QMenu* helpMenu = menuBar()->addMenu("帮助(&H)");
    helpMenu->addAction("关于", this, [this]() {
        QMessageBox::about(this, "关于",
            "CAN/UDS 总线分析与诊断工具\n\n"
            "版本: 1.0.0\n\n"
            "功能:\n"
            "- CAN总线报文监控与过滤\n"
            "- DBC文件解析与信号解析\n"
            "- ISO-TP多帧传输\n"
            "- UDS诊断服务\n"
            "- 信号波形实时绘制\n"
            "- 日志记录与回放");
    });
}

void MainWindow::setupToolbar()
{
    QToolBar* toolBar = addToolBar("主工具栏");
    toolBar->setMovable(false);

    m_actionStartStop = new QAction("启动", this);
    m_actionStartStop->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    toolBar->addAction(m_actionStartStop);

    toolBar->addSeparator();

    toolBar->addAction(m_actionLoadDbc);
    toolBar->addAction(m_actionRecord);
    toolBar->addAction(m_actionLoadLog);
}

void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel("就绪");
    statusBar()->addWidget(m_statusLabel, 1);

    m_statsLabel = new QLabel("帧: 0 | 负载: 0%");
    statusBar()->addPermanentWidget(m_statsLabel);
}

void MainWindow::setupDockWidgets()
{
    // Trace面板 - 报文列表（中央区域）
    m_tracePanel = new TracePanel(this);
    setCentralWidget(m_tracePanel);

    // 配置面板 - 左侧
    QDockWidget* configDock = new QDockWidget("配置", this);
    m_configPanel = new ConfigPanel(configDock);
    configDock->setWidget(m_configPanel);
    configDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, configDock);

    // 信号波形面板 - 底部
    QDockWidget* signalDock = new QDockWidget("信号波形", this);
    m_signalPanel = new SignalPanel(signalDock);
    signalDock->setWidget(m_signalPanel);
    signalDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    signalDock->setMinimumHeight(220);
    addDockWidget(Qt::BottomDockWidgetArea, signalDock);

    // 诊断面板 - 右侧
    QDockWidget* diagDock = new QDockWidget("UDS诊断", this);
    m_diagPanel = new DiagPanel(diagDock);
    diagDock->setWidget(m_diagPanel);
    diagDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, diagDock);

    // 回放面板 - 底部右侧
    QDockWidget* replayDock = new QDockWidget("日志回放", this);
    m_replayPanel = new ReplayPanel(replayDock);
    replayDock->setWidget(m_replayPanel);
    replayDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    replayDock->setMinimumHeight(180);
    addDockWidget(Qt::BottomDockWidgetArea, replayDock);

    // 发送面板 - 左侧底部
    QDockWidget* sendDock = new QDockWidget("报文发送", this);
    m_sendPanel = new SendPanel(sendDock);
    sendDock->setWidget(m_sendPanel);
    sendDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, sendDock);

    // 信号数值面板 - 右侧底部
    QDockWidget* signalValueDock = new QDockWidget("信号数值", this);
    m_signalValuePanel = new SignalValuePanel(signalValueDock);
    signalValueDock->setWidget(m_signalValuePanel);
    signalValueDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, signalValueDock);

    // 统计面板 - 右侧
    QDockWidget* statsDock = new QDockWidget("总线统计", this);
    m_statsPanel = new StatsPanel(statsDock);
    statsDock->setWidget(m_statsPanel);
    statsDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, statsDock);

    // 菜单中添加视图切换
    QMenu* viewMenu = menuBar()->actions()[1]->menu();
    viewMenu->addSeparator();
    viewMenu->addAction(configDock->toggleViewAction());
    viewMenu->addAction(signalDock->toggleViewAction());
    viewMenu->addAction(diagDock->toggleViewAction());
    viewMenu->addAction(replayDock->toggleViewAction());
    viewMenu->addAction(sendDock->toggleViewAction());
    viewMenu->addAction(signalValueDock->toggleViewAction());
    viewMenu->addAction(statsDock->toggleViewAction());
}

void MainWindow::connectSignals()
{
    // 工具栏动作
    connect(m_actionStartStop, &QAction::triggered, this, &MainWindow::onStartStop);
    connect(m_actionLoadDbc, &QAction::triggered, this, &MainWindow::onLoadDbc);
    connect(m_actionRecord, &QAction::triggered, this, &MainWindow::onStartRecording);
    connect(m_actionLoadLog, &QAction::triggered, this, &MainWindow::onLoadLog);

    // CAN管理器
    connect(m_canManager, &CanManager::frameReceived, this, &MainWindow::onFrameReceived);
    connect(m_canManager, &CanManager::filteredFrameReceived, this, &MainWindow::onFilteredFrameReceived);
    connect(m_canManager, &CanManager::adapterStateChanged, this, &MainWindow::onAdapterStateChanged);
    connect(m_canManager, &CanManager::errorOccurred, this, [this](const QString& err) {
        QMessageBox::warning(this, "错误", err);
    });

    // 发送面板
    connect(m_sendPanel, &SendPanel::sendFrameRequested, m_canManager, &CanManager::sendFrame);

    // DBC管理器
    connect(m_dbcManager, &DbcManager::dbcLoaded, this, &MainWindow::onDbcLoaded);

    // 诊断管理器
    connect(m_diagManager, &DiagManager::responseReceived, this, &MainWindow::onDiagResponse);

    // 日志管理器
    connect(m_logManager, &LogManager::recordingStarted, this, [this](bool success) {
        if (success) {
            m_actionRecord->setText("停止记录");
            m_statusLabel->setText("正在记录: " + m_logManager->currentFilePath());
        }
    });
    connect(m_logManager, &LogManager::recordingStopped, this, [this]() {
        m_actionRecord->setText("开始记录");
        m_statusLabel->setText("记录已停止");
    });

    // 统计定时器
    connect(m_statsTimer, &QTimer::timeout, this, &MainWindow::onStatsUpdate);

    // 配置面板
    connect(m_configPanel, &ConfigPanel::startRequested, this, [this](CanManager::AdapterType type, const QString& iface, uint32_t bitrate) {
        m_canManager->startAdapter(type, iface, bitrate);
    });
    connect(m_configPanel, &ConfigPanel::stopRequested, this, [this]() {
        m_canManager->stopAdapter();
    });
    connect(m_configPanel, &ConfigPanel::filterChanged, this, [this]() {
        // 过滤规则变更后刷新显示
    });

    // 诊断面板
    connect(m_diagPanel, &DiagPanel::sendDiagnosticSession, this, [this](uint8_t session) {
        m_diagManager->diagnosticSessionControl(session);
    });
    connect(m_diagPanel, &DiagPanel::sendReadDid, this, [this](uint16_t did) {
        m_diagManager->readDataByIdentifier(did);
    });
    connect(m_diagPanel, &DiagPanel::sendWriteDid, this, [this](uint16_t did, const QByteArray& data) {
        m_diagManager->writeDataByIdentifier(did, data);
    });
    connect(m_diagPanel, &DiagPanel::sendRoutineControl, this, [this](uint8_t type, uint16_t rid, const QByteArray& params) {
        m_diagManager->routineControl(type, rid, params);
    });
    connect(m_diagPanel, &DiagPanel::sendEcuReset, this, [this]() {
        m_diagManager->ecuReset();
    });
    connect(m_diagPanel, &DiagPanel::sendTesterPresent, this, [this]() {
        m_diagManager->testerPresent();
    });
    connect(m_diagPanel, &DiagPanel::txIdChanged, this, [this](uint32_t id) {
        m_diagManager->setTxId(id);
    });
    connect(m_diagPanel, &DiagPanel::rxIdChanged, this, [this](uint32_t id) {
        m_diagManager->setRxId(id);
    });
}

void MainWindow::onStartStop()
{
    if (m_running) {
        m_canManager->stopAdapter();
    } else {
        // 默认使用虚拟适配器
        m_canManager->startAdapter(CanManager::AdapterType::Virtual, "vcan0", 500000);
    }
}

void MainWindow::onLoadDbc()
{
    QString fileName = QFileDialog::getOpenFileName(this, "选择DBC文件", QString(),
        "DBC文件 (*.dbc);;所有文件 (*.*)");
    if (!fileName.isEmpty()) {
        m_dbcManager->loadDbcFile(fileName);
    }
}

void MainWindow::onStartRecording()
{
    if (m_logManager->isRecording()) {
        m_logManager->stopRecording();
    } else {
        QString fileName = QFileDialog::getSaveFileName(this, "保存日志文件", QString(),
            "ASC日志文件 (*.asc);;所有文件 (*.*)");
        if (!fileName.isEmpty()) {
            m_logManager->startRecording(fileName);
        }
    }
}

void MainWindow::onStopRecording()
{
    m_logManager->stopRecording();
}

void MainWindow::onLoadLog()
{
    QString fileName = QFileDialog::getOpenFileName(this, "选择日志文件", QString(),
        "ASC日志文件 (*.asc);;所有文件 (*.*)");
    if (!fileName.isEmpty()) {
        m_logManager->loadLogFile(fileName);
    }
}

void MainWindow::onAdapterStateChanged(bool running)
{
    m_running = running;
    if (running) {
        m_actionStartStop->setText("停止");
        m_actionStartStop->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
        m_statusLabel->setText("已连接: " + m_canManager->adapterName());
        m_statsTimer->start();
    } else {
        m_actionStartStop->setText("启动");
        m_actionStartStop->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        m_statusLabel->setText("已断开");
        m_statsTimer->stop();
    }
}

void MainWindow::onFrameReceived(const CanFrame& frame)
{
    // 传给诊断层处理
    m_diagManager->handleCanFrame(frame);

    // 传给日志记录
    if (m_logManager->isRecording()) {
        m_logManager->onFrameReceived(frame);
    }
}

void MainWindow::onFilteredFrameReceived(const CanFrame& frame)
{
    // 更新trace面板
    m_tracePanel->addFrame(frame);

    // 解析DBC信号
    if (m_dbcManager->isLoaded()) {
        auto signalValues = m_dbcManager->parseFrameSignals(frame);
        for (auto it = signalValues.begin(); it != signalValues.end(); ++it) {
            m_signalManager->onSignalValueReceived(it->first, it->second, frame.timestamp_us);
        }
    }
}

void MainWindow::onStatsUpdate()
{
    const CanStats& stats = m_canManager->stats();
    double load = stats.busLoadPercent(500000);
    m_statsLabel->setText(QString("帧: %1 | 负载: %2% | ID数: %3")
        .arg(stats.totalFrames())
        .arg(load, 0, 'f', 1)
        .arg(stats.uniqueIdCount()));

    if (m_statsPanel) {
        m_statsPanel->updateStats(stats, 500000);
    }
}

void MainWindow::onDbcLoaded(bool success)
{
    if (success) {
        m_statusLabel->setText(QString("DBC已加载: %1 个报文, %2 个信号")
            .arg(m_dbcManager->messageCount())
            .arg(m_dbcManager->signalCount()));

        // 自动添加所有信号到波形面板
        auto messages = m_dbcManager->allMessages();
        for (const auto& pair : messages) {
            for (const auto& sig : pair.second.signalList) {
                QString sigName = QString::fromStdString(sig.name);
                m_signalManager->addSignal(sigName, pair.first);
                m_signalManager->setSignalUnit(sigName, QString::fromStdString(sig.unit));
            }
        }
        m_signalPanel->setSignalManager(m_signalManager);
        m_signalValuePanel->setSignalManager(m_signalManager);
    } else {
        QMessageBox::warning(this, "错误", "DBC文件加载失败: " + m_dbcManager->lastError());
    }
}

void MainWindow::onDiagResponse(bool success, uint8_t serviceId, const QByteArray& data, uint8_t nrc)
{
    m_diagPanel->addResponse(success, serviceId, data, nrc);
}
