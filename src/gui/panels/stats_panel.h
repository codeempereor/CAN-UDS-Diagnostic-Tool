#ifndef STATS_PANEL_H
#define STATS_PANEL_H

#include <QDockWidget>
#include <QTableWidget>
#include <QLabel>
#include <QTimer>

class CanStats;

class StatsPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit StatsPanel(QWidget* parent = nullptr);
    ~StatsPanel() override;

    void updateStats(const CanStats& stats, uint32_t bitrate);
    void reset();

private:
    void setupUi();

    QLabel* m_totalFramesLabel;
    QLabel* m_totalBytesLabel;
    QLabel* m_busLoadLabel;
    QLabel* m_uniqueIdsLabel;
    QLabel* m_errorFramesLabel;
    QLabel* m_remoteFramesLabel;
    QTableWidget* m_idTable;
};

#endif // STATS_PANEL_H
