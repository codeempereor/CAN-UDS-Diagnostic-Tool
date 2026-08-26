#include "dbc/dbc_parser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

DbcParser::DbcParser()
    : m_currentMsgId(0)
{
}

bool DbcParser::loadFromFile(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        m_lastError = "Cannot open file: " + filePath;
        return false;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    return loadFromString(ss.str());
}

bool DbcParser::loadFromString(const std::string& content)
{
    m_messages.clear();
    m_lastError.clear();
    return parse(content);
}

const std::map<uint32_t, DbcMessage>& DbcParser::messages() const
{
    return m_messages;
}

const DbcMessage* DbcParser::getMessage(uint32_t id) const
{
    auto it = m_messages.find(id);
    if (it != m_messages.end()) {
        return &it->second;
    }
    return nullptr;
}

size_t DbcParser::messageCount() const
{
    return m_messages.size();
}

size_t DbcParser::signalCount() const
{
    size_t count = 0;
    for (const auto& pair : m_messages) {
        count += pair.second.signalList.size();
    }
    return count;
}

std::string DbcParser::lastError() const
{
    return m_lastError;
}

bool DbcParser::parse(const std::string& content)
{
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty()) continue;

        if (line.substr(0, 3) == "BO_") {
            parseMessageLine(line);
        } else if (line.substr(0, 3) == "SG_") {
            if (m_messages.count(m_currentMsgId)) {
                parseSignalLine(line, m_messages[m_currentMsgId]);
            }
        } else if (line.substr(0, 3) == "CM_") {
            parseCommentLine(line);
        } else if (line.substr(0, 4) == "VAL_") {
            parseValueTableLine(line);
        }
    }

    return !m_messages.empty();
}

bool DbcParser::parseMessageLine(const std::string& line)
{
    // BO_ id name: dlc sender
    auto parts = split(line, ' ');
    if (parts.size() < 5) return false;

    DbcMessage msg;
    msg.id = static_cast<uint32_t>(std::stoul(parts[1]));
    msg.name = parts[2];
    if (!msg.name.empty() && msg.name.back() == ':') {
        msg.name.pop_back();
    }
    msg.dlc = static_cast<uint8_t>(std::stoul(parts[3]));
    msg.sender = parts[4];

    if (msg.id > 0x7FF) {
        msg.extended = true;
    }

    m_messages[msg.id] = msg;
    m_currentMsgId = msg.id;
    return true;
}

bool DbcParser::parseSignalLine(const std::string& line, DbcMessage& msg)
{
    // SG_ name : startBit|bitLength@endianness+sign (factor,offset) [min|max] "unit" receiver
    DbcSignal signal;

    size_t colonPos = line.find(':');
    if (colonPos == std::string::npos) return false;

    std::string namePart = trim(line.substr(3, colonPos - 3));
    signal.name = namePart;

    std::string rest = trim(line.substr(colonPos + 1));

    // 解析 startBit|bitLength@endianness+sign
    size_t atPos = rest.find('@');
    if (atPos == std::string::npos) return false;

    std::string bitInfo = rest.substr(0, atPos);
    size_t pipePos = bitInfo.find('|');
    if (pipePos == std::string::npos) return false;

    signal.startBit = static_cast<uint8_t>(std::stoul(bitInfo.substr(0, pipePos)));
    signal.bitLength = static_cast<uint8_t>(std::stoul(bitInfo.substr(pipePos + 1)));

    char endianChar = rest[atPos + 1];
    signal.byteOrder = (endianChar == '0') ? ByteOrder::Motorola : ByteOrder::Intel;

    char signChar = rest[atPos + 2];
    signal.valueType = (signChar == '-') ? ValueType::Signed : ValueType::Unsigned;

    // 解析 (factor,offset)
    size_t parenStart = rest.find('(');
    size_t parenEnd = rest.find(')');
    if (parenStart != std::string::npos && parenEnd != std::string::npos) {
        std::string factorOffset = rest.substr(parenStart + 1, parenEnd - parenStart - 1);
        size_t commaPos = factorOffset.find(',');
        if (commaPos != std::string::npos) {
            signal.factor = std::stod(trim(factorOffset.substr(0, commaPos)));
            signal.offset = std::stod(trim(factorOffset.substr(commaPos + 1)));
        }
    }

    // 解析 [min|max]
    size_t bracketStart = rest.find('[');
    size_t bracketEnd = rest.find(']');
    if (bracketStart != std::string::npos && bracketEnd != std::string::npos) {
        std::string minMax = rest.substr(bracketStart + 1, bracketEnd - bracketStart - 1);
        size_t pipePos2 = minMax.find('|');
        if (pipePos2 != std::string::npos) {
            signal.minValue = std::stod(trim(minMax.substr(0, pipePos2)));
            signal.maxValue = std::stod(trim(minMax.substr(pipePos2 + 1)));
        }
    }

    // 解析 unit
    size_t quoteStart = rest.find('"');
    size_t quoteEnd = rest.find('"', quoteStart + 1);
    if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
        signal.unit = rest.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
    }

    msg.signalList.push_back(signal);
    return true;
}

bool DbcParser::parseCommentLine(const std::string& line)
{
    // 简化处理：暂不解析详细注释
    return true;
}

bool DbcParser::parseValueTableLine(const std::string& line)
{
    // VAL_ messageId signalName value1 "desc1" value2 "desc2" ... ;
    std::string content = line;

    // 去掉开头的 "VAL_"
    size_t valPos = content.find("VAL_");
    if (valPos == std::string::npos) return false;
    content = content.substr(valPos + 4);

    // 去掉末尾的分号
    size_t semicolon = content.find_last_of(';');
    if (semicolon != std::string::npos) {
        content = content.substr(0, semicolon);
    }
    content = trim(content);

    // 提取 messageId
    size_t spacePos = content.find(' ');
    if (spacePos == std::string::npos) return false;
    uint32_t msgId = static_cast<uint32_t>(std::stoul(trim(content.substr(0, spacePos))));
    content = trim(content.substr(spacePos + 1));

    // 提取 signalName
    spacePos = content.find(' ');
    if (spacePos == std::string::npos) return false;
    std::string signalName = trim(content.substr(0, spacePos));
    content = trim(content.substr(spacePos + 1));

    // 找到对应的message和signal
    auto msgIt = m_messages.find(msgId);
    if (msgIt == m_messages.end()) return false;

    DbcSignal* targetSignal = nullptr;
    for (auto& sig : msgIt->second.signalList) {
        if (sig.name == signalName) {
            targetSignal = &sig;
            break;
        }
    }
    if (!targetSignal) return false;

    // 循环解析 value "description" 对
    while (!content.empty()) {
        // 提取 value（数字）
        spacePos = content.find(' ');
        if (spacePos == std::string::npos) break;
        std::string valueStr = trim(content.substr(0, spacePos));
        content = trim(content.substr(spacePos + 1));

        // 提取 "description"
        size_t quoteStart = content.find('"');
        size_t quoteEnd = content.find('"', quoteStart + 1);
        if (quoteStart == std::string::npos || quoteEnd == std::string::npos) break;
        std::string desc = content.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        content = trim(content.substr(quoteEnd + 1));

        // 存入值表
        try {
            int64_t value = static_cast<int64_t>(std::stoll(valueStr));
            targetSignal->valueDescriptions[value] = desc;
        } catch (...) {
            // 数值解析失败，跳过
        }
    }

    return true;
}

std::string DbcParser::trim(const std::string& s)
{
    auto start = s.begin();
    while (start != s.end() && std::isspace(static_cast<unsigned char>(*start))) {
        start++;
    }
    auto end = s.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(static_cast<unsigned char>(*end)));
    return std::string(start, end + 1);
}

std::vector<std::string> DbcParser::split(const std::string& s, char delim)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delim)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}
