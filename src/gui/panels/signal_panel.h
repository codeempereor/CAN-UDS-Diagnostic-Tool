#ifndef SIGNAL_PANEL_H
#define SIGNAL_PANEL_H

#include <QWidget>
#include <QListWidget>
#include <QSplitter>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <memory>

class SignalManager;

class SignalPlotWidget : public QWidget {
    Q_OBJECT

public:
    explicit SignalPlotWidget(QWidget* parent = nullptr);
    ~SignalPlotWidget() override;

    void setSignalManager(SignalManager* manager);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void drawGrid(QPainter& painter);
    void drawSignals(QPainter& painter);
    void drawAxes(QPainter& painter);

    SignalManager* m_signalManager;
    QTimer* m_refreshTimer;

    double m_yMin;
    double m_yMax;
    int m_marginLeft;
    int m_marginBottom;
    int m_marginTop;
    int m_marginRight;
};

class SignalPanel : public QWidget {
    Q_OBJECT

public:
    explicit SignalPanel(QWidget* parent = nullptr);
    ~SignalPanel() override;

    void setSignalManager(SignalManager* manager);

private slots:
    void onSignalAdded(const QString& name);
    void onSignalRemoved(const QString& name);
    void onSignalItemChanged(QListWidgetItem* item);

private:
    void setupUi();

    SignalPlotWidget* m_plotWidget;
    QListWidget* m_signalList;
    QSplitter* m_splitter;

    SignalManager* m_signalManager;
};

#endif // SIGNAL_PANEL_H
