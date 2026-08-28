#ifndef CAN_FRAME_H
#define CAN_FRAME_H

#include <cstdint>
#include <vector>
#include <chrono>

struct CanFrame {
    uint32_t id = 0;           // CAN帧ID（标准帧11位，扩展帧29位）
    bool extended = false;     // 是否扩展帧
    bool remote = false;       // 是否远程帧
    bool error = false;        // 是否错误帧
    uint8_t dlc = 0;           // 数据长度（0-8）
    std::vector<uint8_t> data; // 数据载荷
    uint64_t timestamp_us = 0; // 时间戳（微秒）

    CanFrame() = default;

    CanFrame(uint32_t id, const std::vector<uint8_t>& data, bool extended = false)
        : id(id), extended(extended), data(data) 
    {
        dlc = static_cast<uint8_t>(data.size());
        auto now = std::chrono::system_clock::now();
        timestamp_us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()).count());
    }

    static uint64_t currentTimestampUs() 
    {
        auto now = std::chrono::system_clock::now();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()).count());
    }
};

#endif // CAN_FRAME_H
