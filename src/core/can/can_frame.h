#ifndef CAN_FRAME_H
#define CAN_FRAME_H

#include <cstdint>
#include <vector>
#include <chrono>

struct CanFrame {
    uint32_t id = 0;
    bool extended = false;
    bool remote = false;
    bool error = false;
    uint8_t dlc = 0;
    std::vector<uint8_t> data;
    uint64_t timestamp_us = 0;

    CanFrame() = default;

    CanFrame(uint32_t id, const std::vector<uint8_t>& data, bool extended = false)
        : id(id), extended(extended), data(data) {
        dlc = static_cast<uint8_t>(data.size());
        auto now = std::chrono::system_clock::now();
        timestamp_us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()).count());
    }

    static uint64_t currentTimestampUs() {
        auto now = std::chrono::system_clock::now();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()).count());
    }
};

#endif // CAN_FRAME_H
