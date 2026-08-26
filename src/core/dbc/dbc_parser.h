#ifndef DBC_PARSER_H
#define DBC_PARSER_H

#include "dbc_message.h"
#include <string>
#include <vector>
#include <map>
#include <cstdint>

class DbcParser {
public:
    DbcParser();

    bool loadFromFile(const std::string& filePath);
    bool loadFromString(const std::string& content);

    const std::map<uint32_t, DbcMessage>& messages() const;
    const DbcMessage* getMessage(uint32_t id) const;

    size_t messageCount() const;
    size_t signalCount() const;

    std::string lastError() const;

private:
    bool parse(const std::string& content);
    bool parseMessageLine(const std::string& line);
    bool parseSignalLine(const std::string& line, DbcMessage& msg);
    bool parseCommentLine(const std::string& line);
    bool parseValueTableLine(const std::string& line);  // VAL_ 值表

    std::string trim(const std::string& s);
    std::vector<std::string> split(const std::string& s, char delim);

    std::map<uint32_t, DbcMessage> m_messages;
    std::string m_lastError;
    uint32_t m_currentMsgId;
};

#endif // DBC_PARSER_H
