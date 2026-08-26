#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <vector>
#include <fstream>

#include "can/can_frame.h"

class LogManager : public QObject {
    Q_OBJECT

public:
    explicit LogManager(QObject* parent = nullptr);
    ~LogManager() override;

    bool startRecording(const QString& filePath);
    void stopRecording();
    bool isRecording() const;

    bool loadLogFile(const QString& filePath);
    const std::vector<CanFrame>& logFrames() const;

    size_t frameCount() const;
    QString currentFilePath() const;

public slots:
    void onFrameReceived(const CanFrame& frame);

signals:
    void recordingStarted(bool success);
    void recordingStopped();
    void logLoaded(size_t frameCount);

private:
    bool writeAscFrame(const CanFrame& frame);

    std::ofstream m_logFile;
    QString m_filePath;
    bool m_recording;
    uint64_t m_startTimeUs;

    std::vector<CanFrame> m_logFrames;
};

#endif // LOG_MANAGER_H
