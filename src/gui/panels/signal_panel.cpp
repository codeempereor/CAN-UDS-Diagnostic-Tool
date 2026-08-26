#include "signal_panel.h"
#include "business/signal_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QScrollBar>

// ==================== SignalPlotWidget ====================

SignalPlotWidget::SignalPlotWidget(QWidget* parent)
    : QWidget(parent)
    , m_signalManager(nullptr)
    , m_refreshTimer(nullptr)
    , m_yMin(0)
    , m_yMax(100)
    , m_marginLeft(60)
    , m_marginBottom(30)
    , m_marginTop(10)
    , m_marginRight(10)
{
    setMinimumHeight(200);
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Base);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(50);
    connect(m_refreshTimer, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
    m_refreshTimer->start();
}

SignalPlotWidget::~SignalPlotWidget() = default;

void SignalPlotWidget::setSignalManager(SignalManager* manager)
{
    m_signalManager = manager;
}

void SignalPlotWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    drawGrid(painter);
    drawAxes(painter);
    drawSignals(painter);
}

void SignalPlotWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
}

void SignalPlotWidget::drawGrid(QPainter& painter)
{
    painter.setPen(QPen(QColor(230, 230, 230), 1, Qt::DotLine));

    int plotWidth = width() - m_marginLeft - m_marginRight;
    int plotHeight = height() - m_marginTop - m_marginBottom;

    // 横向网格线
    int gridCountY = 5;
    for (int i = 0; i <= gridCountY; ++i) {
        int y = m_marginTop + plotHeight * i / gridCountY;
        painter.drawLine(m_marginLeft, y, m_marginLeft + plotWidth, y);
    }

    // 纵向网格线
    int gridCountX = 10;
    for (int i = 0; i <= gridCountX; ++i) {
        int x = m_marginLeft + plotWidth * i / gridCountX;
        painter.drawLine(x, m_marginTop, x, m_marginTop + plotHeight);
    }
}

void SignalPlotWidget::drawAxes(QPainter& painter)
{
    painter.setPen(QPen(QColor(100, 100, 100), 1));

    int plotWidth = width() - m_marginLeft - m_marginRight;
    int plotHeight = height() - m_marginTop - m_marginBottom;

    // Y轴
    painter.drawLine(m_marginLeft, m_marginTop, m_marginLeft, m_marginTop + plotHeight);
    // X轴
    painter.drawLine(m_marginLeft, m_marginTop + plotHeight,
                     m_marginLeft + plotWidth, m_marginTop + plotHeight);

    // Y轴刻度标签
    painter.setPen(QColor(80, 80, 80));
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    int gridCountY = 5;
    for (int i = 0; i <= gridCountY; ++i) {
        int y = m_marginTop + plotHeight * i / gridCountY;
        double val = m_yMax - (m_yMax - m_yMin) * i / gridCountY;
        QString label = QString::number(val, 'f', 1);
        painter.drawText(2, y + 4, label);
    }
}

void SignalPlotWidget::drawSignals(QPainter& painter)
{
    if (!m_signalManager) return;

    int plotWidth = width() - m_marginLeft - m_marginRight;
    int plotHeight = height() - m_marginTop - m_marginBottom;

    QStringList names = m_signalManager->signalNames();
    if (names.isEmpty()) return;

    // 计算Y轴范围
    double globalMin = 0, globalMax = 100;
    bool first = true;
    for (const QString& name : names) {
        const SignalData* data = m_signalManager->signalData(name);
        if (!data || !data->visible || data->values.empty()) continue;
        if (first) {
            globalMin = data->minValue;
            globalMax = data->maxValue;
            first = false;
        } else {
            if (data->minValue < globalMin) globalMin = data->minValue;
            if (data->maxValue > globalMax) globalMax = data->maxValue;
        }
    }

    if (globalMax == globalMin) {
        globalMax = globalMin + 1;
    }

    // 增加一点边距
    double range = globalMax - globalMin;
    m_yMin = globalMin - range * 0.1;
    m_yMax = globalMax + range * 0.1;

    double yRange = m_yMax - m_yMin;

    for (const QString& name : names) {
        const SignalData* data = m_signalManager->signalData(name);
        if (!data || !data->visible || data->values.empty()) continue;

        painter.setPen(QPen(data->color, 1.5));

        QPainterPath path;
        size_t count = data->values.size();
        size_t maxPoints = static_cast<size_t>(plotWidth);
        size_t step = (count > maxPoints) ? (count / maxPoints) : 1;

        for (size_t i = 0; i < count; i += step) {
            double x = m_marginLeft + (double)i / count * plotWidth;
            double y = m_marginTop + plotHeight - (data->values[i] - m_yMin) / yRange * plotHeight;

            if (i == 0) {
                path.moveTo(x, y);
            } else {
                path.lineTo(x, y);
            }
        }

        painter.drawPath(path);
    }

    // 图例
    painter.setPen(QColor(60, 60, 60));
    int legendY = m_marginTop + 10;
    for (const QString& name : names) {
        const SignalData* data = m_signalManager->signalData(name);
        if (!data || !data->visible) continue;

        painter.fillRect(m_marginLeft + 10, legendY, 12, 12, data->color);
        painter.drawText(m_marginLeft + 28, legendY + 10, name);
        legendY += 18;
    }
}

// ==================== SignalPanel ====================

SignalPanel::SignalPanel(QWidget* parent)
    : QWidget(parent)
    , m_plotWidget(nullptr)
    , m_signalList(nullptr)
    , m_splitter(nullptr)
    , m_signalManager(nullptr)
{
    setupUi();
}

SignalPanel::~SignalPanel() = default;

void SignalPanel::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);

    m_splitter = new QSplitter(Qt::Horizontal, this);

    // 左侧信号列表
    QWidget* listWidget = new QWidget(this);
    QVBoxLayout* listLayout = new QVBoxLayout(listWidget);
    listLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* listLabel = new QLabel("信号列表", this);
    listLayout->addWidget(listLabel);

    m_signalList = new QListWidget(this);
    m_signalList->setMaximumWidth(180);
    connect(m_signalList, &QListWidget::itemChanged, this, &SignalPanel::onSignalItemChanged);
    listLayout->addWidget(m_signalList, 1);

    m_splitter->addWidget(listWidget);

    // 右侧波形图
    m_plotWidget = new SignalPlotWidget(this);
    m_splitter->addWidget(m_plotWidget);

    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(m_splitter, 1);
}

void SignalPanel::setSignalManager(SignalManager* manager)
{
    m_signalManager = manager;
    m_plotWidget->setSignalManager(manager);

    connect(m_signalManager, &SignalManager::signalAdded, this, &SignalPanel::onSignalAdded);
    connect(m_signalManager, &SignalManager::signalRemoved, this, &SignalPanel::onSignalRemoved);

    // 添加已有信号
    QStringList names = m_signalManager->signalNames();
    for (const QString& name : names) {
        onSignalAdded(name);
    }
}

void SignalPanel::onSignalAdded(const QString& name)
{
    QListWidgetItem* item = new QListWidgetItem(name, m_signalList);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Checked);

    const SignalData* data = m_signalManager->signalData(name);
    if (data) {
        item->setForeground(QBrush(data->color));
    }
}

void SignalPanel::onSignalRemoved(const QString& name)
{
    for (int i = 0; i < m_signalList->count(); ++i) {
        if (m_signalList->item(i)->text() == name) {
            delete m_signalList->takeItem(i);
            break;
        }
    }
}

void SignalPanel::onSignalItemChanged(QListWidgetItem* item)
{
    if (!m_signalManager) return;
    QString name = item->text();
    bool visible = (item->checkState() == Qt::Checked);
    m_signalManager->setSignalVisible(name, visible);
}
