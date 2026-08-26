#include "dbc_manager.h"
#include <QFile>
#include <QTextStream>

DbcManager::DbcManager(QObject* parent)
    : QObject(parent)
    , m_parser(std::make_unique<DbcParser>())
    , m_loaded(false)
{
}

DbcManager::~DbcManager() = default;

bool DbcManager::loadDbcFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit dbcLoaded(false);
        return false;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    bool success = loadDbcString(content);
    if (success) {
        m_filePath = filePath;
    }
    return success;
}

bool DbcManager::loadDbcString(const QString& content)
{
    bool success = m_parser->loadFromString(content.toStdString());
    m_loaded = success;
    emit dbcLoaded(success);
    return success;
}

void DbcManager::clear()
{
    m_parser = std::make_unique<DbcParser>();
    m_filePath.clear();
    m_loaded = false;
    emit dbcCleared();
}

bool DbcManager::isLoaded() const
{
    return m_loaded;
}

QString DbcManager::filePath() const
{
    return m_filePath;
}

size_t DbcManager::messageCount() const
{
    return m_parser->messageCount();
}

size_t DbcManager::signalCount() const
{
    return m_parser->signalCount();
}

const DbcMessage* DbcManager::getMessage(uint32_t id) const
{
    return m_parser->getMessage(id);
}

std::map<uint32_t, DbcMessage> DbcManager::allMessages() const
{
    return m_parser->messages();
}

std::map<QString, double> DbcManager::parseFrameSignals(const CanFrame& frame) const
{
    std::map<QString, double> result;

    if (!m_loaded) return result;

    const DbcMessage* msg = m_parser->getMessage(frame.id);
    if (!msg) return result;

    auto values = msg->getAllValues(frame.data.data(), frame.dlc);
    for (const auto& pair : values) {
        result[QString::fromStdString(pair.first)] = pair.second;
    }

    return result;
}

QString DbcManager::lastError() const
{
    return QString::fromStdString(m_parser->lastError());
}
