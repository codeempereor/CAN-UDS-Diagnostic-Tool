#ifndef DBC_MANAGER_H
#define DBC_MANAGER_H

#include <QObject>
#include <QString>
#include <memory>
#include <map>

#include "dbc/dbc_parser.h"
#include "can/can_frame.h"

class DbcManager : public QObject {
    Q_OBJECT

public:
    explicit DbcManager(QObject* parent = nullptr);
    ~DbcManager() override;

    bool loadDbcFile(const QString& filePath);
    bool loadDbcString(const QString& content);
    void clear();

    bool isLoaded() const;
    QString filePath() const;

    size_t messageCount() const;
    size_t signalCount() const;

    const DbcMessage* getMessage(uint32_t id) const;
    std::map<uint32_t, DbcMessage> allMessages() const;

    // 解析CAN帧的所有信号值
    std::map<QString, double> parseFrameSignals(const CanFrame& frame) const;

    QString lastError() const;

signals:
    void dbcLoaded(bool success);
    void dbcCleared();

private:
    std::unique_ptr<DbcParser> m_parser;
    QString m_filePath;
    bool m_loaded;
};

#endif // DBC_MANAGER_H
