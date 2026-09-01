#ifndef TRACE_PANEL_H
#define TRACE_PANEL_H

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTimer>

#include "can/can_frame.h"

class TracePanel : public QWidget {
    Q_OBJECT

public:
    explicit TracePanel(QWidget* parent = nullptr);
    ~TracePanel() override;

    void addFrame(const CanFrame& frame);
    void clear();

    void setFilterId(const QString& idFilter);
    bool exportToCsv(const QString& filePath);
    void setPaused(bool paused);
    bool isPaused() const { return m_paused; }

private slots:
    void onFilterTextChanged(const QString& text);
    void onPauseToggled(bool checked);
    void onClearClicked();
    void onExportClicked();
    void updateDisplay();

private:
    void setupUi();
    QString formatData(const std::vector<uint8_t>& data);

    QTableWidget* m_table;
    QLineEdit* m_filterEdit;
    QCheckBox* m_pauseCheck;
    QPushButton* m_clearBtn;
    QPushButton* m_exportBtn;
    QLabel* m_countLabel;

    QList<CanFrame> m_frames;
    QList<CanFrame> m_pendingFrames;
    QTimer* m_updateTimer;

    bool m_paused;
    QString m_filterText;
    uint64_t m_frameCount;
};

#endif // TRACE_PANEL_H
