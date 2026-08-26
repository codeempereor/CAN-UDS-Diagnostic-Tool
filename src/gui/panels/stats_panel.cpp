#include "stats_panel.h"
#include "can/can_stats.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>

StatsPanel::StatsPanel(QWidget* parent)
    : QDockWidget("总线统计", parent)
{
    setupUi();
}

StatsPanel::~StatsPanel() = default;

void StatsPanel::setupUi()
{
    auto* widget = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(widget);

    // 统计信息卡片
    auto* statsGroup = new QGroupBox("总线状态", this);
    auto* statsLayout = new QGridLayout(statsGroup);

    m_totalFramesLabel = new QLabel("0", this);
    m_totalBytesLabel = new QLabel("0 B", this);
    m_busLoadLabel = new QLabel("0.0%", this);
    m_uniqueIdsLabel = new QLabel("0", this);
    m_errorFramesLabel = new QLabel("0", this);
    m_remoteFramesLabel = new QLabel("0", this);

    auto addStat = [&](int row, int col, const QString& title, QLabel* value) {
        auto* titleLabel = new QLabel(title, this);
        titleLabel->setStyleSheet("color: #666; font-size: 11px;");
        value->setStyleSheet("font-size: 16px; font-weight: bold;");
        statsLayout->addWidget(titleLabel, row * 2, col);
        statsLayout->addWidget(value, row * 2 + 1, col);
    };

    addStat(0, 0, "总帧数", m_totalFramesLabel);
    addStat(0, 1, "总字节", m_totalBytesLabel);
    addStat(0, 2, "负载率", m_busLoadLabel);
    addStat(1, 0, "唯一ID数", m_uniqueIdsLabel);
    addStat(1, 1, "错误帧", m_errorFramesLabel);
    addStat(1, 2, "远程帧", m_remoteFramesLabel);

    mainLayout->addWidget(statsGroup);

    // ID分布表格
    auto* idGroup = new QGroupBox("ID分布", this);
    auto* idLayout = new QVBoxLayout(idGroup);
    m_idTable = new QTableWidget(this);
    m_idTable->setColumnCount(4);
    m_idTable->setHorizontalHeaderLabels({"CAN ID", "帧数", "占比", "柱状图"});
    m_idTable->horizontalHeader()->setStretchLastSection(true);
    m_idTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_idTable->setAlternatingRowColors(true);
    m_idTable->verticalHeader()->setVisible(false);
    idLayout->addWidget(m_idTable);
    mainLayout->addWidget(idGroup, 1);

    setWidget(widget);
}

void StatsPanel::updateStats(const CanStats& stats, uint32_t bitrate)
{
    m_totalFramesLabel->setText(QString::number(stats.totalFrames()));

    uint64_t bytes = stats.totalBytes();
    QString byteStr;
    if (bytes < 1024) byteStr = QString("%1 B").arg(bytes);
    else if (bytes < 1024 * 1024) byteStr = QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    else byteStr = QString("%1 MB").arg(bytes / (1024.0 * 1024), 0, 'f', 2);
    m_totalBytesLabel->setText(byteStr);

    double load = stats.busLoadPercent(bitrate);
    m_busLoadLabel->setText(QString("%1%").arg(load, 0, 'f', 1));
    if (load > 80) m_busLoadLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: red;");
    else if (load > 50) m_busLoadLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: orange;");
    else m_busLoadLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: green;");

    m_uniqueIdsLabel->setText(QString::number(stats.uniqueIdCount()));
    m_errorFramesLabel->setText(QString::number(stats.errorFrames()));
    m_remoteFramesLabel->setText(QString::number(stats.remoteFrames()));

    // 更新ID分布表
    auto idDist = stats.idDistribution();
    m_idTable->setRowCount(0);

    uint64_t total = stats.totalFrames();
    if (total == 0) total = 1;

    // 按帧数降序排序
    std::vector<std::pair<uint32_t, uint64_t>> sortedIds(idDist.begin(), idDist.end());
    std::sort(sortedIds.begin(), sortedIds.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    uint64_t maxCount = sortedIds.empty() ? 1 : sortedIds[0].second;

    for (const auto& pair : sortedIds) {
        int row = m_idTable->rowCount();
        m_idTable->insertRow(row);

        m_idTable->setItem(row, 0, new QTableWidgetItem(QString("0x%1").arg(pair.first, 3, 16, QChar('0')).toUpper()));
        m_idTable->setItem(row, 1, new QTableWidgetItem(QString::number(pair.second)));

        double percent = (double)pair.second / total * 100.0;
        m_idTable->setItem(row, 2, new QTableWidgetItem(QString("%1%").arg(percent, 0, 'f', 1)));

        // 简单柱状图（用方块字符）
        int barLen = (int)((double)pair.second / maxCount * 20);
        QString bar = QString(barLen, QChar(0x2588));  // 实心方块
        auto* barItem = new QTableWidgetItem(bar);
        barItem->setForeground(QBrush(QColor(52, 152, 219)));
        m_idTable->setItem(row, 3, barItem);
    }
}
