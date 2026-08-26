#include "log_manager.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QRegularExpression>
#include <sstream>
#include <iomanip>

LogManager::LogManager(QObject* parent)
    : QObject(parent)
    , m_recording(false)
    , m_startTimeUs(0)
{
}

LogManager::~LogManager()
{
    stopRecording();
}

bool LogManager::startRecording(const QString& filePath)
{
    if (m_recording) {
        stopRecording();
    }

    m_logFile.open(filePath.toStdString(), std::ios::out | std::ios::trunc);
    if (!m_logFile.is_open()) {
        emit recordingStarted(false);
        return false;
    }

    m_filePath = filePath;
    m_recording = true;
    m_startTimeUs = 0;

    // 写入ASC文件头
    m_logFile << "date " << QDateTime::currentDateTime().toString("ddd MMM d HH:mm:ss yyyy").toStdString() << "\n";
    m_logFile << "base hex  timestamps absolute\n";
    m_logFile << "no internal events logged\n";
    m_logFile << "// version 12.0.0\n";
    m_logFile << "Begin Triggerblock\n";
    m_logFile << " 0.000000 Start of measurement\n";

    emit recordingStarted(true);
    return true;
}

void LogManager::stopRecording()
{
    if (m_logFile.is_open()) {
        m_logFile << "End Triggerblock\n";
        m_logFile.close();
    }
    m_recording = false;
    emit recordingStopped();
}

bool LogManager::isRecording() const
{
    return m_recording;
}

bool LogManager::loadLogFile(const QString& filePath)
{
    m_logFrames.clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit logLoaded(0);
        return false;
    }

    QTextStream in(&file);
    QString line;
    bool inTriggerBlock = false;

    while (!in.atEnd()) {
        line = in.readLine().trimmed();

        if (line.startsWith("Begin Triggerblock")) {
            inTriggerBlock = true;
            continue;
        }
        if (line.startsWith("End Triggerblock")) {
            inTriggerBlock = false;
            break;
        }

        if (!inTriggerBlock || line.isEmpty()) continue;

        // 简单解析ASC格式的CAN帧
        // 格式: 时间 接口  ID  Rx/Dx  DLC  data...
        QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 5) continue;

        bool ok;
        double timestamp = parts[0].toDouble(&ok);
        if (!ok) continue;

        // 找ID和数据
        int idIdx = -1;
        int dlcIdx = -1;
        for (int i = 1; i < parts.size(); ++i) {
            if (parts[i] == "Rx" || parts[i] == "Tx") {
                idIdx = i - 1;
                dlcIdx = i + 1;
                break;
            }
        }

        if (idIdx < 0 || dlcIdx < 0 || dlcIdx >= parts.size()) continue;

        uint32_t id = parts[idIdx].toUInt(&ok, 16);
        if (!ok) continue;

        uint8_t dlc = static_cast<uint8_t>(parts[dlcIdx].toUInt(&ok));
        if (!ok) continue;

        CanFrame frame;
        frame.id = id;
        frame.dlc = dlc;
        frame.timestamp_us = static_cast<uint64_t>(timestamp * 1000000);

        for (uint8_t i = 0; i < dlc && dlcIdx + 1 + i < parts.size(); ++i) {
            uint8_t byte = static_cast<uint8_t>(parts[dlcIdx + 1 + i].toUInt(&ok, 16));
            if (ok) {
                frame.data.push_back(byte);
            }
        }

        m_logFrames.push_back(frame);
    }

    file.close();
    emit logLoaded(m_logFrames.size());
    return true;
}

const std::vector<CanFrame>& LogManager::logFrames() const
{
    return m_logFrames;
}

size_t LogManager::frameCount() const
{
    return m_logFrames.size();
}

QString LogManager::currentFilePath() const
{
    return m_filePath;
}

void LogManager::onFrameReceived(const CanFrame& frame)
{
    if (!m_recording || !m_logFile.is_open()) return;

    writeAscFrame(frame);
}

bool LogManager::writeAscFrame(const CanFrame& frame)
{
    if (m_startTimeUs == 0) {
        m_startTimeUs = frame.timestamp_us;
    }

    double timeOffset = (frame.timestamp_us - m_startTimeUs) / 1000000.0;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << timeOffset << " ";
    oss << "1 "; // 通道号
    oss << std::hex << std::uppercase << std::setw(3) << std::setfill(' ') << frame.id;
    oss << " Rx ";
    oss << std::dec << static_cast<int>(frame.dlc) << "  ";

    for (size_t i = 0; i < frame.data.size(); ++i) {
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
            << static_cast<int>(frame.data[i]) << " ";
    }

    oss << "\n";
    m_logFile << oss.str();
    m_logFile.flush();

    return true;
}
