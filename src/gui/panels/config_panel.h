#ifndef CONFIG_PANEL_H
#define CONFIG_PANEL_H

#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QLabel>

#include "business/can_manager.h"

class ConfigPanel : public QWidget {
    Q_OBJECT

public:
    explicit ConfigPanel(QWidget* parent = nullptr);
    ~ConfigPanel() override;

signals:
    void startRequested(CanManager::AdapterType type, const QString& interface, uint32_t bitrate);
    void stopRequested();
    void filterChanged();

private slots:
    void onStartClicked();
    void onStopClicked();
    void onAdapterTypeChanged(int index);

private:
    void setupUi();

    QGroupBox* m_hardwareGroup;
    QComboBox* m_adapterCombo;
    QLineEdit* m_interfaceEdit;
    QComboBox* m_bitrateCombo;
    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;

    QGroupBox* m_filterGroup;
    QLineEdit* m_filterEdit;
    QComboBox* m_filterModeCombo;
    QPushButton* m_applyFilterBtn;

    bool m_started;
};

#endif // CONFIG_PANEL_H
