#ifndef DBC_MESSAGE_H
#define DBC_MESSAGE_H

#include "dbc_signal.h"
#include <string>
#include <vector>
#include <cstdint>
#include <map>

struct DbcMessage {
    uint32_t id = 0;
    bool extended = false;
    std::string name;
    uint8_t dlc = 8;
    std::string sender;
    std::string comment;
    std::vector<DbcSignal> signalList;
    std::map<std::string, std::string> attributes;

    const DbcSignal* findSignal(const std::string& name) const {
        for (const auto& sig : signalList) {
            if (sig.name == name) return &sig;
        }
        return nullptr;
    }

    // 根据信号名和原始值获取枚举描述文本
    std::string getSignalValueDescription(const std::string& signalName, int64_t rawValue) const {
        const DbcSignal* sig = findSignal(signalName);
        if (sig) {
            return sig->getValueDescription(rawValue);
        }
        return std::string();
    }

    uint64_t extractRawValue(const uint8_t* data, uint8_t dataLen, const DbcSignal& signal) const {
        if (signal.startBit + signal.bitLength > dataLen * 8) {
            return 0;
        }

        uint64_t raw = 0;
        if (signal.byteOrder == ByteOrder::Intel) {
            // 小端：低位在前
            for (int i = signal.bitLength - 1; i >= 0; --i) {
                uint8_t bitPos = signal.startBit + i;
                uint8_t byteIdx = bitPos / 8;
                uint8_t bitIdx = bitPos % 8;
                raw = (raw << 1) | ((data[byteIdx] >> bitIdx) & 0x01);
            }
        } else {
            // 大端(Motorola)：MSB在前，从startBit开始
            // 字节内位号递减(7→0)，跨字节跳到下一字节的bit7
            uint8_t byteIdx = signal.startBit / 8;
            int8_t bitIdx = static_cast<int8_t>(signal.startBit % 8);
            for (uint8_t i = 0; i < signal.bitLength; ++i) {
                raw = (raw << 1) | ((data[byteIdx] >> bitIdx) & 0x01);
                bitIdx--;
                if (bitIdx < 0) {
                    byteIdx++;
                    bitIdx = 7;
                }
            }
        }
        return raw;
    }

    double getPhysicalValue(const uint8_t* data, uint8_t dataLen, const std::string& signalName) const {
        const DbcSignal* sig = findSignal(signalName);
        if (!sig) return 0.0;
        uint64_t raw = extractRawValue(data, dataLen, *sig);
        return sig->rawToPhysical(raw);
    }

    std::map<std::string, double> getAllValues(const uint8_t* data, uint8_t dataLen) const {
        std::map<std::string, double> result;
        for (const auto& sig : signalList) {
            uint64_t raw = extractRawValue(data, dataLen, sig);
            result[sig.name] = sig.rawToPhysical(raw);
        }
        return result;
    }
};

#endif // DBC_MESSAGE_H
