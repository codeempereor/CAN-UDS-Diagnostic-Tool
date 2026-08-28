#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QAction>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QDockWidget>
#include <QTimer>
#include <memory>

#include "business/can_manager.h"
#include "business/dbc_manager.h"
#include "business/diag_manager.h"
#include "business/log_manager.h"
#include "business/signal_manager.h"

class TracePanel;
class SignalPanel;
class DiagPanel;
class ConfigPanel;
class ReplayPanel;
class SendPanel;
class SignalValuePanel;
class StatsPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onStartStop();
    void onLoadDbc();
    void onStartRecording();
    void onStopRecording();
    void onLoadLog();

    void onAdapterStateChanged(bool running);
    void onFrameReceived(const CanFrame& frame);
    void onFilteredFrameReceived(const CanFrame& frame);
    void onStatsUpdate();

    void onDbcLoaded(bool success);
    void onDiagResponse(bool success, uint8_t serviceId, const QByteArray& data, uint8_t nrc);

private:
    void setupUi();
    void setupMenus();
    void setupToolbar();
    void setupStatusBar();
    void setupDockWidgets();
    void connectSignals();

    // 管理器
    CanManager* m_canManager;
    DbcManager* m_dbcManager;
    DiagManager* m_diagManager;
    LogManager* m_logManager;
    SignalManager* m_signalManager;

    // 面板
    TracePanel* m_tracePanel;
    SignalPanel* m_signalPanel;
    DiagPanel* m_diagPanel;
    ConfigPanel* m_configPanel;
    ReplayPanel* m_replayPanel;
    SendPanel* m_sendPanel;
    SignalValuePanel* m_signalValuePanel;
    StatsPanel* m_statsPanel;

    // 动作
    QAction* m_actionStartStop;
    QAction* m_actionLoadDbc;
    QAction* m_actionRecord;
    QAction* m_actionLoadLog;

    // 状态栏
    QLabel* m_statusLabel;
    QLabel* m_statsLabel;
    QTimer* m_statsTimer;

    // 右侧折叠边栏
    QToolBar* m_rightSidebar;

    bool m_running;
};

#endif // MAIN_WINDOW_H
