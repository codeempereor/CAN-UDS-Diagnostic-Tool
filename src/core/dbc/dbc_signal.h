#ifndef DBC_SIGNAL_H
#define DBC_SIGNAL_H

#include <string>
#include <cstdint>
#include <map>

enum class ByteOrder {
    Motorola, // 大端
    Intel     // 小端
};

enum class ValueType {
    Signed,
    Unsigned,
    Float
};

struct DbcSignal {
    std::string name;
    uint8_t startBit = 0;
    uint8_t bitLength = 0;
    ByteOrder byteOrder = ByteOrder::Intel;
    ValueType valueType = ValueType::Unsigned;
    double factor = 1.0;
    double offset = 0.0;
    double minValue = 0.0;
    double maxValue = 0.0;
    std::string unit;
    std::string comment;

    // VAL_值表：原始值 -> 描述文本（枚举信号）
    std::map<int64_t, std::string> valueDescriptions;

    // 根据原始值获取描述文本，没有匹配返回空字符串
    std::string getValueDescription(int64_t rawValue) const {
        auto it = valueDescriptions.find(rawValue);
        if (it != valueDescriptions.end()) {
            return it->second;
        }
        return std::string();
    }

    bool hasValueTable() const {
        return !valueDescriptions.empty();
    }

    double rawToPhysical(uint64_t raw) const {
        double val = 0;
        if (valueType == ValueType::Signed) {
            int64_t signedRaw = static_cast<int64_t>(raw);
            if (bitLength < 64 && (raw & (1ULL << (bitLength - 1)))) {
                signedRaw |= ~((1ULL << bitLength) - 1);
            }
            val = static_cast<double>(signedRaw);
        } else {
            val = static_cast<double>(raw);
        }
        return val * factor + offset;
    }

    uint64_t physicalToRaw(double physical) const {
        double raw = (physical - offset) / factor;
        if (raw < 0) raw = 0;
        uint64_t mask = (bitLength >= 64) ? ~0ULL : ((1ULL << bitLength) - 1);
        return static_cast<uint64_t>(raw) & mask;
    }
};

#endif // DBC_SIGNAL_H
