#include "trace_panel.h"
#include <QPushButton>
#include <QHeaderView>
#include <QScrollBar>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <iomanip>
#include <sstream>

TracePanel::TracePanel(QWidget* parent)
    : QWidget(parent)
    , m_table(nullptr)
    , m_filterEdit(nullptr)
    , m_pauseCheck(nullptr)
    , m_clearBtn(nullptr)
    , m_countLabel(nullptr)
    , m_updateTimer(nullptr)
    , m_paused(false)
    , m_frameCount(0)
{
    setupUi();
}

TracePanel::~TracePanel() = default;

void TracePanel::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    // 工具栏
    QHBoxLayout* toolLayout = new QHBoxLayout();

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("过滤ID (如: 123, 0x7E0)");
    toolLayout->addWidget(m_filterEdit, 1);

    m_pauseCheck = new QCheckBox("暂停", this);
    toolLayout->addWidget(m_pauseCheck);

    m_clearBtn = new QPushButton("清空", this);
    toolLayout->addWidget(m_clearBtn);

    m_exportBtn = new QPushButton("导出CSV", this);
    toolLayout->addWidget(m_exportBtn);

    m_countLabel = new QLabel("0 帧", this);
    toolLayout->addWidget(m_countLabel);

    mainLayout->addLayout(toolLayout);

    // 表格
    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({"序号", "时间(ms)", "ID", "DLC", "数据"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);

    // 设置列宽
    m_table->setColumnWidth(0, 60);
    m_table->setColumnWidth(1, 100);
    m_table->setColumnWidth(2, 80);
    m_table->setColumnWidth(3, 50);

    mainLayout->addWidget(m_table, 1);

    // 定时器批量更新
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(50);
    connect(m_updateTimer, &QTimer::timeout, this, &TracePanel::updateDisplay);
    m_updateTimer->start();

    // 信号连接
    connect(m_filterEdit, &QLineEdit::textChanged, this, &TracePanel::onFilterTextChanged);
    connect(m_pauseCheck, &QCheckBox::toggled, this, &TracePanel::onPauseToggled);
    connect(m_clearBtn, &QPushButton::clicked, this, &TracePanel::onClearClicked);
    connect(m_exportBtn, &QPushButton::clicked, this, &TracePanel::onExportClicked);
}

void TracePanel::addFrame(const CanFrame& frame)
{
    m_pendingFrames.append(frame);
    m_frameCount++;
}

void TracePanel::clear()
{
    m_frames.clear();
    m_pendingFrames.clear();
    m_frameCount = 0;
    m_table->setRowCount(0);
    m_countLabel->setText("0 帧");
}

void TracePanel::setFilterId(const QString& idFilter)
{
    m_filterEdit->setText(idFilter);
}

void TracePanel::onFilterTextChanged(const QString& text)
{
    m_filterText = text.trimmed();
}

void TracePanel::onPauseToggled(bool checked)
{
    m_paused = checked;
}

void TracePanel::onClearClicked()
{
    clear();
}

void TracePanel::onExportClicked()
{
    QString filePath = QFileDialog::getSaveFileName(this, "导出CSV", "can_trace.csv", "CSV Files (*.csv)");
    if (filePath.isEmpty()) return;
    if (exportToCsv(filePath)) {
        QMessageBox::information(this, "导出成功", QString("已导出 %1 帧到:\n%2").arg(m_frames.size()).arg(filePath));
    } else {
        QMessageBox::warning(this, "导出失败", "文件写入失败，请检查路径权限");
    }
}

bool TracePanel::exportToCsv(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // CSV表头
    out << "No.,Timestamp(us),ID,Extended,DLC,Data\n";

    for (int i = 0; i < m_frames.size(); ++i) {
        const CanFrame& frame = m_frames[i];
        out << i + 1 << ","
            << frame.timestamp_us << ","
            << "0x" << QString::number(frame.id, 16).toUpper() << ","
            << (frame.extended ? "1" : "0") << ","
            << static_cast<int>(frame.dlc) << ","
            << "\"" << formatData(frame.data) << "\"\n";
    }

    file.close();
    return true;
}

void TracePanel::updateDisplay()
{
    if (m_paused || m_pendingFrames.isEmpty()) return;

    bool scrollToBottom = m_table->verticalScrollBar()->value()
                          >= m_table->verticalScrollBar()->maximum() - 10;

    for (const auto& frame : m_pendingFrames) {
        // 过滤
        if (!m_filterText.isEmpty()) {
            bool ok;
            uint32_t filterId = m_filterText.toUInt(&ok, 0);
            if (ok && frame.id != filterId) {
                continue;
            }
        }

        int row = m_table->rowCount();
        m_table->insertRow(row);

        // 序号
        m_table->setItem(row, 0, new QTableWidgetItem(QString::number(m_frameCount - m_pendingFrames.size() + row + 1)));

        // 时间戳
        double timeMs = frame.timestamp_us / 1000.0;
        m_table->setItem(row, 1, new QTableWidgetItem(QString::number(timeMs, 'f', 3)));

        // ID
        QString idStr = QString("0x%1").arg(frame.id, frame.extended ? 8 : 3, 16, QChar('0')).toUpper();
        QTableWidgetItem* idItem = new QTableWidgetItem(idStr);
        if (frame.id >= 0x700 && frame.id <= 0x7FF) {
            idItem->setForeground(QBrush(QColor(0x00, 0x66, 0xCC))); // 诊断帧蓝色
        }
        m_table->setItem(row, 2, idItem);

        // DLC
        m_table->setItem(row, 3, new QTableWidgetItem(QString::number(frame.dlc)));

        // 数据
        m_table->setItem(row, 4, new QTableWidgetItem(formatData(frame.data)));

        m_frames.append(frame);
    }

    m_pendingFrames.clear();
    m_countLabel->setText(QString("%1 帧").arg(m_frameCount));

    if (scrollToBottom) {
        m_table->scrollToBottom();
    }

    // 限制最大行数
    if (m_table->rowCount() > 5000) {
        while (m_table->rowCount() > 5000) {
            m_table->removeRow(0);
        }
    }
}

QString TracePanel::formatData(const std::vector<uint8_t>& data)
{
    QStringList bytes;
    for (uint8_t b : data) {
        bytes.append(QString("%1").arg(b, 2, 16, QChar('0')).toUpper());
    }
    return bytes.join(" ");
}


void TracePanel::setPaused(bool paused)
{
    m_paused = paused;
    if (m_pauseCheck) m_pauseCheck->setChecked(paused);
}
