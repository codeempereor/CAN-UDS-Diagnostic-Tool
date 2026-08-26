#include "signal_value_panel.h"
#include "business/signal_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QPushButton>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>

SignalValuePanel::SignalValuePanel(QWidget* parent)
    : QDockWidget("信号数值", parent)
    , m_signalManager(nullptr)
{
    setupUi();
}

SignalValuePanel::~SignalValuePanel() = default;

void SignalValuePanel::setupUi()
{
    auto* widget = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(widget);

    // 过滤框 + 导出按钮
    auto* filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel("过滤:"));
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("输入信号名过滤...");
    connect(m_filterEdit, &QLineEdit::textChanged, this, &SignalValuePanel::onFilterTextChanged);
    filterLayout->addWidget(m_filterEdit, 1);
    m_exportBtn = new QPushButton("导出CSV", this);
    connect(m_exportBtn, &QPushButton::clicked, this, &SignalValuePanel::onExportClicked);
    filterLayout->addWidget(m_exportBtn);
    mainLayout->addLayout(filterLayout);

    // 表格
    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({"信号名", "当前值", "单位", "原始值", "描述"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    mainLayout->addWidget(m_table);

    setWidget(widget);
}

void SignalValuePanel::setSignalManager(SignalManager* manager)
{
    if (m_signalManager) {
        disconnect(m_signalManager, &SignalManager::signalAdded, this, nullptr);
        disconnect(m_signalManager, &SignalManager::dataUpdated, this, nullptr);
    }
    m_signalManager = manager;
    if (m_signalManager) {
        connect(m_signalManager, &SignalManager::signalAdded, this, [this](const QString& name) {
            addSignalRow(name);
        });
        connect(m_signalManager, &SignalManager::dataUpdated, this, &SignalValuePanel::refreshValues);
    }
}

void SignalValuePanel::addSignalRow(const QString& name)
{
    if (m_rowMap.contains(name)) return;

    int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, 0, new QTableWidgetItem(name));
    m_table->setItem(row, 1, new QTableWidgetItem("--"));
    m_table->setItem(row, 2, new QTableWidgetItem(""));
    m_table->setItem(row, 3, new QTableWidgetItem("--"));
    m_table->setItem(row, 4, new QTableWidgetItem(""));
    m_rowMap[name] = row;

    // 应用过滤
    onFilterTextChanged(m_filterEdit->text());
}

void SignalValuePanel::refreshValues()
{
    if (!m_signalManager) return;

    for (auto it = m_rowMap.begin(); it != m_rowMap.end(); ++it) {
        updateRowValue(it.value(), it.key());
    }
}

void SignalValuePanel::updateRowValue(int row, const QString& name)
{
    if (!m_signalManager) return;

    const SignalData* sig = m_signalManager->signalData(name);
    if (!sig || sig->values.empty()) return;

    double currentValue = sig->values.back();
    QString valueStr = QString::number(currentValue, 'f', 3);

    if (m_table->item(row, 1)) {
        m_table->item(row, 1)->setText(valueStr);
    }
    if (m_table->item(row, 2)) {
        m_table->item(row, 2)->setText(sig->unit);
    }
}

void SignalValuePanel::clear()
{
    m_table->setRowCount(0);
    m_rowMap.clear();
}

void SignalValuePanel::onFilterTextChanged(const QString& text)
{
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QString name = m_table->item(row, 0)->text();
        bool match = text.isEmpty() || name.contains(text, Qt::CaseInsensitive);
        m_table->setRowHidden(row, !match);
    }
}

void SignalValuePanel::onExportClicked()
{
    QString filePath = QFileDialog::getSaveFileName(this, "导出信号数据", "signal_values.csv", "CSV Files (*.csv)");
    if (filePath.isEmpty()) return;
    if (exportToCsv(filePath)) {
        QMessageBox::information(this, "导出成功", QString("已导出 %1 个信号到:\n%2").arg(m_table->rowCount()).arg(filePath));
    } else {
        QMessageBox::warning(this, "导出失败", "文件写入失败，请检查路径权限");
    }
}

bool SignalValuePanel::exportToCsv(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "SignalName,CurrentValue,Unit,RawValue,Description\n";

    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (m_table->isRowHidden(row)) continue;
        QString name = m_table->item(row, 0)->text();
        QString value = m_table->item(row, 1)->text();
        QString unit = m_table->item(row, 2)->text();
        QString raw = m_table->item(row, 3)->text();
        QString desc = m_table->item(row, 4)->text();
        out << "\"" << name << "\","
            << value << ","
            << "\"" << unit << "\","
            << raw << ","
            << "\"" << desc << "\"\n";
    }

    file.close();
    return true;
}
