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
#include <QInputDialog>
#include <QSettings>
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
    // 1. 文件菜单
    QMenu* fileMenu = menuBar()->addMenu("文件(&F)");
    m_actionLoadDbc = new QAction("加载DBC文件...", this);
    m_actionLoadDbc->setShortcut(QKeySequence("Ctrl+D"));
    fileMenu->addAction(m_actionLoadDbc);
    m_actionLoadLog = new QAction("加载日志文件...", this);
    m_actionLoadLog->setShortcut(QKeySequence("Ctrl+O"));
    fileMenu->addAction(m_actionLoadLog);
    fileMenu->addSeparator();
    QAction* exportTraceAction = new QAction("导出报文CSV...", this);
    exportTraceAction->setShortcut(QKeySequence("Ctrl+E"));
    connect(exportTraceAction, &QAction::triggered, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, "导出报文CSV", "frames.csv", "CSV Files (*.csv)");
        if (!path.isEmpty() && m_tracePanel) {
            bool ok = m_tracePanel->exportToCsv(path);
            QMessageBox::information(this, "导出", ok ? "报文CSV导出成功" : "导出失败");
        }
    });
    fileMenu->addAction(exportTraceAction);
    QAction* exportSignalAction = new QAction("导出信号CSV...", this);
    connect(exportSignalAction, &QAction::triggered, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, "导出信号CSV", "signals.csv", "CSV Files (*.csv)");
        if (!path.isEmpty() && m_signalValuePanel) {
            bool ok = m_signalValuePanel->exportToCsv(path);
            QMessageBox::information(this, "导出", ok ? "信号CSV导出成功" : "导出失败");
        }
    });
    fileMenu->addAction(exportSignalAction);
    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction("退出", this, &QWidget::close);
    exitAction->setShortcut(QKeySequence("Ctrl+Q"));

    // 2. 硬件菜单
    QMenu* hwMenu = menuBar()->addMenu("硬件(&H)");
    QAction* adapterConfigAction = new QAction("适配器配置...", this);
    adapterConfigAction->setShortcut(QKeySequence("Ctrl+Shift+H"));
    connect(adapterConfigAction, &QAction::triggered, this, [this]() {
        if (m_configPanel) {
            m_configPanel->setFocus();
            m_configPanel->raise();
            QMessageBox::information(this, "适配器配置", "请在左侧「硬件配置」面板中设置适配器类型、接口名和波特率，然后点击「启动」。");
        }
    });
    hwMenu->addAction(adapterConfigAction);
    QMenu* bitrateMenu = hwMenu->addMenu("波特率");
    QMap<QString, uint32_t> bitrateMap;
    bitrateMap["125 kbps"] = 125000;
    bitrateMap["250 kbps"] = 250000;
    bitrateMap["500 kbps"] = 500000;
    bitrateMap["1 Mbps"] = 1000000;
    for (auto it = bitrateMap.begin(); it != bitrateMap.end(); ++it) {
        QAction* brAction = bitrateMenu->addAction(it.key());
        connect(brAction, &QAction::triggered, this, [this, it]() {
            if (m_canManager && m_canManager->isRunning()) {
                QMessageBox::information(this, "波特率", QString("当前适配器正在运行，请先停止后在配置面板选择 %1 再启动。").arg(it.key()));
            } else {
                QMessageBox::information(this, "波特率", QString("已选择 %1，请在配置面板确认后启动适配器。").arg(it.key()));
            }
        });
    }
    hwMenu->addSeparator();
    QAction* hwFilterAction = new QAction("硬件过滤配置...", this);
    connect(hwFilterAction, &QAction::triggered, this, [this]() {
        if (m_configPanel) {
            m_configPanel->setFocus();
            QMessageBox::information(this, "硬件过滤", "硬件过滤功能开发中，当前支持软件ID过滤（左侧配置面板「ID过滤」区域）。");
        }
    });
    hwMenu->addAction(hwFilterAction);

    // 3. 测量菜单
    QMenu* measureMenu = menuBar()->addMenu("测量(&M)");
    measureMenu->addAction(m_actionStartStop);
    QAction* pauseAction = new QAction("暂停显示", this);
    pauseAction->setShortcut(QKeySequence("Ctrl+P"));
    pauseAction->setCheckable(true);
    connect(pauseAction, &QAction::triggered, this, [this, pauseAction]() {
        if (m_tracePanel) m_tracePanel->setPaused(pauseAction->isChecked());
    });
    measureMenu->addAction(pauseAction);
    measureMenu->addSeparator();
    m_actionRecord = new QAction("开始记录", this);
    m_actionRecord->setShortcut(QKeySequence("Ctrl+R"));
    measureMenu->addAction(m_actionRecord);
    measureMenu->addSeparator();
    QAction* clearAction = new QAction("清空报文", this);
    clearAction->setShortcut(QKeySequence("Ctrl+L"));
    connect(clearAction, &QAction::triggered, this, [this]() {
        if (m_tracePanel) m_tracePanel->clear();
    });
    measureMenu->addAction(clearAction);

    // 4. 分析菜单
    QMenu* analysisMenu = menuBar()->addMenu("分析(&A)");
    QAction* dbcManageAction = new QAction("DBC信号管理...", this);
    connect(dbcManageAction, &QAction::triggered, this, [this]() {
        if (m_signalManager && m_signalManager->signalCount() > 0) {
            QString info = QString("已加载DBC信息:\n\n信号数: %1\n\n信号列表:\n").arg(m_signalManager->signalCount());
            QMessageBox::information(this, "DBC信号管理", info);
        } else {
            QMessageBox::information(this, "DBC信号管理", "当前未加载DBC文件，请通过「文件 → 加载DBC文件」加载。");
        }
    });
    analysisMenu->addAction(dbcManageAction);
    QAction* waveformConfigAction = new QAction("信号波形设置...", this);
    connect(waveformConfigAction, &QAction::triggered, this, [this]() {
        QMessageBox::information(this, "波形设置", "请在底部「信号波形」面板中勾选信号以显示/隐藏波形。\n\n当前支持:\n- 多信号同时显示\n- 实时曲线绘制\n- 自动缩放Y轴");
    });
    analysisMenu->addAction(waveformConfigAction);
    analysisMenu->addSeparator();
    QAction* filterConfigAction = new QAction("ID过滤器配置...", this);
    connect(filterConfigAction, &QAction::triggered, this, [this]() {
        if (m_configPanel) {
            m_configPanel->setFocus();
            QMessageBox::information(this, "ID过滤", "请在左侧「ID过滤」区域配置过滤模式（显示/隐藏）和过滤ID（逗号分隔）。");
        }
    });
    analysisMenu->addAction(filterConfigAction);
    QAction* resetStatsAction = new QAction("重置统计", this);
    connect(resetStatsAction, &QAction::triggered, this, [this]() {
        if (m_statsPanel) m_statsPanel->reset();
        QMessageBox::information(this, "重置统计", "总线统计已重置。");
    });
    analysisMenu->addAction(resetStatsAction);

    // 5. 视图菜单
    QMenu* viewMenu = menuBar()->addMenu("视图(&V)");
    viewMenu->addAction("重置布局", this, [this]() {
        for (QDockWidget* dock : findChildren<QDockWidget*>()) {
            dock->setVisible(true);
            dock->setFloating(false);
        }
    });
    QAction* saveLayoutAction = new QAction("保存布局", this);
    connect(saveLayoutAction, &QAction::triggered, this, [this]() {
        QSettings settings("CAN_UDS_Tool", "Layout");
        settings.setValue("mainWindowState", saveState());
        settings.setValue("mainWindowGeometry", saveGeometry());
        QMessageBox::information(this, "保存布局", "当前布局已保存。");
    });
    viewMenu->addAction(saveLayoutAction);
    QAction* loadLayoutAction = new QAction("加载布局", this);
    connect(loadLayoutAction, &QAction::triggered, this, [this]() {
        QSettings settings("CAN_UDS_Tool", "Layout");
        if (settings.contains("mainWindowState")) {
            restoreState(settings.value("mainWindowState").toByteArray());
            restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
            QMessageBox::information(this, "加载布局", "布局已恢复。");
        } else {
            QMessageBox::information(this, "加载布局", "未找到已保存的布局。");
        }
    });
    viewMenu->addAction(loadLayoutAction);
    viewMenu->addSeparator();
    QAction* fullscreenAction = new QAction("全屏", this);
    fullscreenAction->setShortcut(QKeySequence("F11"));
    fullscreenAction->setCheckable(true);
    connect(fullscreenAction, &QAction::triggered, this, [this, fullscreenAction]() {
        fullscreenAction->isChecked() ? showFullScreen() : showNormal();
    });
    viewMenu->addAction(fullscreenAction);
    viewMenu->addSeparator();
    viewMenu->addAction("显示工具栏", this, [this]() {
        for (QToolBar* tb : findChildren<QToolBar*>()) tb->show();
    });
    viewMenu->addAction("隐藏工具栏", this, [this]() {
        for (QToolBar* tb : findChildren<QToolBar*>()) tb->hide();
    });

    // 6. 诊断菜单
    QMenu* diagMenu = menuBar()->addMenu("诊断(&D)");
    QMenu* sessionMenu = diagMenu->addMenu("会话控制");
    QMap<QString, uint8_t> sessionMap;
    sessionMap["默认会话 (0x01)"] = 0x01;
    sessionMap["编程会话 (0x02)"] = 0x02;
    sessionMap["扩展会话 (0x03)"] = 0x03;
    for (auto it = sessionMap.begin(); it != sessionMap.end(); ++it) {
        QAction* sAction = sessionMenu->addAction(it.key());
        connect(sAction, &QAction::triggered, this, [this, it]() {
            if (m_diagManager) {
                bool ok = m_diagManager->diagnosticSessionControl(it.value());
                QMessageBox::information(this, "会话控制", ok ? QString("已发送 %1 请求").arg(it.key()) : "发送失败，请检查适配器是否已启动");
            }
        });
    }
    diagMenu->addAction("读DID...", this, [this]() {
        bool ok;
        QString didStr = QInputDialog::getText(this, "读DID", "输入DID (十六进制, 如 F190):", QLineEdit::Normal, "", &ok);
        if (ok && !didStr.isEmpty() && m_diagManager) {
            uint16_t did = didStr.toUInt(&ok, 16);
            if (ok) {
                m_diagManager->readDataByIdentifier(did);
                QMessageBox::information(this, "读DID", QString("已发送读DID请求: 0x%1").arg(did, 4, 16, QChar('0')).toUpper());
            }
        }
    });
    diagMenu->addAction("写DID...", this, [this]() {
        bool ok;
        QString didStr = QInputDialog::getText(this, "写DID", "输入DID (十六进制):", QLineEdit::Normal, "", &ok);
        if (ok && !didStr.isEmpty()) {
            QString dataStr = QInputDialog::getText(this, "写DID", "输入数据 (十六进制, 如 010203):", QLineEdit::Normal, "", &ok);
            if (ok && m_diagManager) {
                uint16_t did = didStr.toUInt(&ok, 16);
                QByteArray data = QByteArray::fromHex(dataStr.toLatin1());
                if (ok) {
                    m_diagManager->writeDataByIdentifier(did, data);
                    QMessageBox::information(this, "写DID", QString("已发送写DID请求: 0x%1").arg(did, 4, 16, QChar('0')).toUpper());
                }
            }
        }
    });
    diagMenu->addAction("例程控制...", this, [this]() {
        bool ok;
        QString ridStr = QInputDialog::getText(this, "例程控制", "输入RID (十六进制):", QLineEdit::Normal, "", &ok);
        if (ok && !ridStr.isEmpty() && m_diagManager) {
            uint16_t rid = ridStr.toUInt(&ok, 16);
            if (ok) {
                m_diagManager->routineControl(0x01, rid);
                QMessageBox::information(this, "例程控制", QString("已发送例程控制请求: 0x%1").arg(rid, 4, 16, QChar('0')).toUpper());
            }
        }
    });
    diagMenu->addSeparator();
    QAction* securityAction = new QAction("安全访问...", this);
    connect(securityAction, &QAction::triggered, this, [this]() {
        if (m_diagManager) {
            m_diagManager->diagnosticSessionControl(0x03);
            QMessageBox::information(this, "安全访问", "已切换到扩展会话 (0x03)。\n\n安全访问流程:\n1. 请求种子 (0x27 01)\n2. 计算密钥\n3. 发送密钥 (0x27 02)\n\n请在右侧「UDS诊断」面板中操作。");
        }
    });
    diagMenu->addAction(securityAction);
    QAction* testerPresentAction = new QAction("TesterPresent保活", this);
    testerPresentAction->setCheckable(true);
    connect(testerPresentAction, &QAction::triggered, this, [this, testerPresentAction]() {
        if (m_diagManager) {
            m_diagManager->setTesterPresentEnabled(testerPresentAction->isChecked());
            QMessageBox::information(this, "TesterPresent", QString("TesterPresent保活: %1").arg(testerPresentAction->isChecked() ? "已开启" : "已关闭"));
        }
    });
    diagMenu->addAction(testerPresentAction);

    // 7. 工具菜单
    QMenu* toolsMenu = menuBar()->addMenu("工具(&T)");
    QAction* optionsAction = new QAction("选项设置...", this);
    optionsAction->setShortcut(QKeySequence("Ctrl+,"));
    connect(optionsAction, &QAction::triggered, this, [this]() {
        QMessageBox::information(this, "选项设置", "选项设置功能开发中。\n\n当前可配置项:\n- 适配器类型 (配置面板)\n- 波特率 (配置面板)\n- ID过滤 (配置面板)\n- 信号显示 (波形面板)");
    });
    toolsMenu->addAction(optionsAction);
    toolsMenu->addSeparator();
    QAction* crcAction = new QAction("CRC计算器...", this);
    connect(crcAction, &QAction::triggered, this, [this]() {
        bool ok;
        QString dataStr = QInputDialog::getText(this, "CRC计算器", "输入数据 (十六进制, 如 01020304):", QLineEdit::Normal, "", &ok);
        if (ok && !dataStr.isEmpty()) {
            QByteArray data = QByteArray::fromHex(dataStr.toLatin1());
            uint8_t crc8 = 0;
            for (uint8_t b : data) {
                crc8 ^= b;
                for (int i = 0; i < 8; i++) {
                    crc8 = (crc8 & 0x80) ? (crc8 << 1) ^ 0x07 : (crc8 << 1);
                }
            }
            uint16_t crc16 = 0xFFFF;
            for (uint8_t b : data) {
                crc16 ^= (uint16_t)b << 8;
                for (int i = 0; i < 8; i++) {
                    crc16 = (crc16 & 0x8000) ? (crc16 << 1) ^ 0x1021 : (crc16 << 1);
                }
            }
            QMessageBox::information(this, "CRC计算结果",
                QString("输入数据: %1\n\nCRC-8 (0x07): 0x%2\nCRC-16 (CCITT): 0x%3")
                .arg(dataStr.toUpper())
                .arg(crc8, 2, 16, QChar('0')).toUpper()
                .arg(crc16, 4, 16, QChar('0')).toUpper());
        }
    });
    toolsMenu->addAction(crcAction);
    QAction* bitfieldAction = new QAction("位域计算器...", this);
    connect(bitfieldAction, &QAction::triggered, this, [this]() {
        bool ok;
        QString valStr = QInputDialog::getText(this, "位域计算器", "输入32位值 (十六进制, 如 1A2B3C4D):", QLineEdit::Normal, "", &ok);
        if (ok && !valStr.isEmpty()) {
            uint32_t val = valStr.toUInt(&ok, 16);
            if (ok) {
                QString bits;
                for (int i = 31; i >= 0; i--) {
                    bits += (val & (1u << i)) ? '1' : '0';
                    if (i % 8 == 0 && i != 0) bits += ' ';
                }
                QMessageBox::information(this, "位域计算结果",
                    QString("输入: 0x%1\n\n二进制:\n%2\n\n字节0 (MSB): 0x%3\n字节1: 0x%4\n字节2: 0x%5\n字节3 (LSB): 0x%6")
                    .arg(val, 8, 16, QChar('0')).toUpper()
                    .arg(bits)
                    .arg((val >> 24) & 0xFF, 2, 16, QChar('0')).toUpper()
                    .arg((val >> 16) & 0xFF, 2, 16, QChar('0')).toUpper()
                    .arg((val >> 8) & 0xFF, 2, 16, QChar('0')).toUpper()
                    .arg(val & 0xFF, 2, 16, QChar('0')).toUpper());
            }
        }
    });
    toolsMenu->addAction(bitfieldAction);
    toolsMenu->addSeparator();
    QAction* checkUpdateAction = new QAction("检查更新", this);
    connect(checkUpdateAction, &QAction::triggered, this, [this]() {
        QMessageBox::information(this, "检查更新", "当前版本 v1.1.0\n\nGitHub: https://github.com/codeempereor/CAN-UDS-Diagnostic-Tool");
    });
    toolsMenu->addAction(checkUpdateAction);

    // 8. 帮助菜单
    QMenu* helpMenu = menuBar()->addMenu("帮助(&H)");
    QAction* docAction = new QAction("使用文档", this);
    docAction->setShortcut(QKeySequence("F1"));
    connect(docAction, &QAction::triggered, this, [this]() {
        QMessageBox::information(this, "使用文档",
            "CAN/UDS 总线分析与诊断工具 - 使用文档\n\n"
            "快速开始:\n"
            "1. 左侧选择「虚拟适配器(模拟)」\n"
            "2. 点击「启动」按钮\n"
            "3. 工具栏点击「加载DBC文件」\n"
            "4. 查看报文监控、信号波形、总线统计\n"
            "5. 右侧边栏展开UDS诊断面板\n\n"
            "快捷键:\n"
            "Ctrl+D  加载DBC\n"
            "Ctrl+O  加载日志\n"
            "Ctrl+E  导出报文CSV\n"
            "Ctrl+R  开始记录\n"
            "Ctrl+P  暂停显示\n"
            "Ctrl+L  清空报文\n"
            "Ctrl+,  选项设置\n"
            "F11     全屏\n"
            "F1      使用文档\n"
            "Space   启动/停止");
    });
    helpMenu->addAction(docAction);
    QAction* techDocAction = new QAction("技术文档", this);
    connect(techDocAction, &QAction::triggered, this, [this]() {
        QMessageBox::information(this, "技术文档",
            "项目包含5份深度技术文档，位于 docs/ 目录:\n\n"
            "01_设计理念与架构总览.md\n"
            "02_核心协议栈_CAN与DBC.md\n"
            "03_核心协议栈_ISO-TP与UDS.md\n"
            "04_硬件抽象层与业务编排层.md\n"
            "05_GUI层与问题总结.md\n\n"
            "以及项目综合报告 reports/ 目录。");
    });
    helpMenu->addAction(techDocAction);
    helpMenu->addSeparator();
    helpMenu->addAction("关于", this, [this]() {
        QMessageBox::about(this, "关于",
            "<h3>CAN/UDS 总线分析与诊断工具</h3>"
            "<p>版本: 1.1.0</p>"
            "<p>从0到1自研的车载CAN总线分析与UDS诊断工具</p>"
            "<p><b>核心功能:</b></p>"
            "<ul><li>CAN总线报文监控与过滤</li><li>DBC文件解析与信号转换</li>"
            "<li>ISO-TP多帧传输 (ISO 15765-2)</li><li>UDS诊断服务 (ISO 14229-1)</li>"
            "<li>信号波形实时绘制</li><li>总线统计与ID分布</li><li>日志记录与回放</li></ul>"
            "<p><b>技术栈:</b> C++17 / Qt6 / CMake</p>"
            "<p>作者: 三道渊 (codeempereor)</p>");
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

    // 状态栏右侧统计信息已移除
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
    m_rightSidebar->setFixedWidth(32);
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
            btn->setMinimumHeight(80);
            btn->setCursor(Qt::PointingHandCursor);
        }
    }

    // 边栏样式
    m_rightSidebar->setStyleSheet(R"(
        QToolBar {
            background-color: #f5f5f5;
            border-left: 1px solid #d9d9d9;
            spacing: 2px;
            padding: 2px 0px;
        }
        QToolButton {
            background-color: transparent;
            border: none;
            border-radius: 4px;
            padding: 4px 0px;
            font-size: 11px;
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
    if (m_statsLabel) m_statsLabel->setText(QString("帧: %1 | 负载: %2% | ID数: %3")
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
