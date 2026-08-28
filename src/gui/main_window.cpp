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
#include <QToolBar>
#include <QToolButton>
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
    // ============================================================
    // 中央区域：报文监控（Trace）— 核心面板，占最大空间
    // ============================================================
    m_tracePanel = new TracePanel(this);
    setCentralWidget(m_tracePanel);

    // ============================================================
    // 左侧区域：配置面板（上）+ 报文发送（下）
    // ============================================================

    // 配置面板 - 左侧上方
    QDockWidget* configDock = new QDockWidget("硬件配置", this);
    m_configPanel = new ConfigPanel(configDock);
    configDock->setWidget(m_configPanel);
    configDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    configDock->setMinimumWidth(220);
    configDock->setMinimumHeight(280);
    addDockWidget(Qt::LeftDockWidgetArea, configDock);

    // 报文发送 - 左侧下方（SendPanel继承自QDockWidget，直接使用）
    m_sendPanel = new SendPanel(this);
    m_sendPanel->setWindowTitle("报文发送");
    m_sendPanel->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_sendPanel->setMinimumWidth(220);
    m_sendPanel->setMinimumHeight(200);
    addDockWidget(Qt::LeftDockWidgetArea, m_sendPanel);
    // 把报文发送放在配置面板下方，分割比例 3:2
    splitDockWidget(configDock, m_sendPanel, Qt::Vertical);

    // ============================================================
    // 右侧区域：UDS诊断 + 信号数值 + 总线统计（标签化，可折叠）
    // ============================================================

    // UDS诊断 - 右侧
    QDockWidget* diagDock = new QDockWidget("UDS诊断", this);
    m_diagPanel = new DiagPanel(diagDock);
    diagDock->setWidget(m_diagPanel);
    diagDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    diagDock->setMinimumWidth(280);
    diagDock->setMinimumHeight(260);
    addDockWidget(Qt::RightDockWidgetArea, diagDock);

    // 信号数值 - 右侧（SignalValuePanel继承自QDockWidget，直接使用）
    m_signalValuePanel = new SignalValuePanel(this);
    m_signalValuePanel->setWindowTitle("信号数值");
    m_signalValuePanel->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    m_signalValuePanel->setMinimumWidth(280);
    m_signalValuePanel->setMinimumHeight(200);
    addDockWidget(Qt::RightDockWidgetArea, m_signalValuePanel);

    // 总线统计 - 右侧（StatsPanel继承自QDockWidget，直接使用）
    m_statsPanel = new StatsPanel(this);
    m_statsPanel->setWindowTitle("总线统计");
    m_statsPanel->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    m_statsPanel->setMinimumWidth(280);
    m_statsPanel->setMinimumHeight(180);
    addDockWidget(Qt::RightDockWidgetArea, m_statsPanel);

    // 三个右侧面板标签化（共用一个区域，用标签切换）
    tabifyDockWidget(diagDock, m_signalValuePanel);
    tabifyDockWidget(m_signalValuePanel, m_statsPanel);
    diagDock->raise(); // 默认显示UDS诊断标签

    // 默认折叠隐藏右侧面板
    diagDock->hide();
    m_signalValuePanel->hide();
    m_statsPanel->hide();

    // ============================================================
    // 底部区域：信号波形（左，占65%）+ 日志回放（右，占35%）
    // ============================================================

    // 信号波形 - 底部左侧
    QDockWidget* signalDock = new QDockWidget("信号波形", this);
    m_signalPanel = new SignalPanel(signalDock);
    signalDock->setWidget(m_signalPanel);
    signalDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    signalDock->setMinimumHeight(240);
    addDockWidget(Qt::BottomDockWidgetArea, signalDock);

    // 日志回放 - 底部右侧
    QDockWidget* replayDock = new QDockWidget("日志回放", this);
    m_replayPanel = new ReplayPanel(replayDock);
    replayDock->setWidget(m_replayPanel);
    replayDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    replayDock->setMinimumHeight(180);
    addDockWidget(Qt::BottomDockWidgetArea, replayDock);
    // 把日志回放在信号波形右边，水平分割
    splitDockWidget(signalDock, replayDock, Qt::Horizontal);

    // ============================================================
    // 设置初始大小比例
    // ============================================================

    // 左侧区域宽度 260px
    resizeDocks({configDock, m_sendPanel}, {260, 260}, Qt::Horizontal);
    // 右侧区域宽度 300px
    resizeDocks({diagDock, m_signalValuePanel, m_statsPanel}, {300, 300, 300}, Qt::Horizontal);
    // 底部区域高度 280px
    resizeDocks({signalDock, replayDock}, {280, 280}, Qt::Vertical);
    // 底部水平分割：信号波形占65%，日志回放占35%
    resizeDocks({signalDock, replayDock}, {650, 350}, Qt::Horizontal);

    // ============================================================
    // 视图菜单：添加所有面板的显示/隐藏切换
    // ============================================================
    QMenu* viewMenu = menuBar()->actions()[1]->menu();
    viewMenu->addSeparator();
    viewMenu->addAction(configDock->toggleViewAction());
    viewMenu->addAction(m_sendPanel->toggleViewAction());
    viewMenu->addAction(diagDock->toggleViewAction());
    viewMenu->addAction(m_signalValuePanel->toggleViewAction());
    viewMenu->addAction(m_statsPanel->toggleViewAction());
    viewMenu->addAction(signalDock->toggleViewAction());
    viewMenu->addAction(replayDock->toggleViewAction());

    // ============================================================
    // 右侧折叠边栏：点击按钮展开/收起右侧面板
    // ============================================================
    m_rightSidebar = new QToolBar("右侧面板", this);
    m_rightSidebar->setMovable(false);
    m_rightSidebar->setFloatable(false);
    m_rightSidebar->setOrientation(Qt::Vertical);
    m_rightSidebar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_rightSidebar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_rightSidebar->setFixedWidth(38);
    addToolBar(Qt::RightToolBarArea, m_rightSidebar);

    // 添加三个面板的切换按钮
    m_rightSidebar->addAction(diagDock->toggleViewAction());
    m_rightSidebar->addAction(m_signalValuePanel->toggleViewAction());
    m_rightSidebar->addAction(m_statsPanel->toggleViewAction());

    // 设置按钮文字垂直显示 + 点击时切换到对应标签
    const QList<QAction*> sidebarActions = {
        diagDock->toggleViewAction(),
        m_signalValuePanel->toggleViewAction(),
        m_statsPanel->toggleViewAction()
    };
    const QList<QDockWidget*> sidebarDocks = {
        diagDock, m_signalValuePanel, m_statsPanel
    };
    for (int i = 0; i < sidebarActions.size(); ++i) {
        QAction* act = sidebarActions[i];
        QDockWidget* dock = sidebarDocks[i];
        // 文字换行实现垂直显示
        QString text = act->text();
        QString verticalText;
        for (QChar c : text) {
            verticalText += c;
            verticalText += '\n';
        }
        act->setText(verticalText);
        // 点击显示时切换到对应标签
        connect(act, &QAction::toggled, this, [dock](bool visible) {
            if (visible) dock->raise();
        });
    }

    // 设置边栏按钮样式
    for (QObject* obj : m_rightSidebar->children()) {
        QToolButton* btn = qobject_cast<QToolButton*>(obj);
        if (btn) {
            btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            btn->setMinimumHeight(90);
            btn->setCursor(Qt::PointingHandCursor);
        }
    }

    // 边栏样式
    m_rightSidebar->setStyleSheet(R"(
        QToolBar {
            background-color: #f5f5f5;
            border-left: 1px solid #d9d9d9;
            spacing: 3px;
            padding: 4px 2px;
        }
        QToolButton {
            background-color: transparent;
            border: none;
            border-radius: 4px;
            padding: 6px 2px;
            font-size: 12px;
            color: #555;
            line-height: 1.3;
        }
        QToolButton:hover {
            background-color: #e8e8e8;
            color: #222;
        }
        QToolButton:checked {
            background-color: #d6e4ff;
            color: #1a73e8;
            font-weight: bold;
        }
    )");
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
