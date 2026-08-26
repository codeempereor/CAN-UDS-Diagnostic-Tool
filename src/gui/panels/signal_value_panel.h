#ifndef SIGNAL_VALUE_PANEL_H
#define SIGNAL_VALUE_PANEL_H

#include <QDockWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QMap>
#include <QString>

class SignalManager;

class SignalValuePanel : public QDockWidget {
    Q_OBJECT

public:
    explicit SignalValuePanel(QWidget* parent = nullptr);
    ~SignalValuePanel() override;

    void setSignalManager(SignalManager* manager);
    bool exportToCsv(const QString& filePath);

public slots:
    void refreshValues();
    void clear();
    void onExportClicked();

private slots:
    void onFilterTextChanged(const QString& text);

private:
    void setupUi();
    void addSignalRow(const QString& name);
    void updateRowValue(int row, const QString& name);

    QTableWidget* m_table;
    QLineEdit* m_filterEdit;
    QPushButton* m_exportBtn;
    SignalManager* m_signalManager;
    QMap<QString, int> m_rowMap;  // 信号名 -> 行号
};

#endif // SIGNAL_VALUE_PANEL_H
