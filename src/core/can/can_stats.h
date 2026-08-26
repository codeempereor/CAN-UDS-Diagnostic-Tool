#ifndef CAN_STATS_H
#define CAN_STATS_H

#include "can/can_frame.h"
#include <cstdint>
#include <map>
#include <mutex>

class CanStats {
public:
    CanStats();

    void reset();
    void update(const CanFrame& frame);

    uint64_t totalFrames() const;
    uint64_t totalBytes() const;
    uint64_t errorFrames() const;
    uint64_t remoteFrames() const;

    double busLoadPercent(uint32_t bitrate_bps) const;
    uint32_t uniqueIdCount() const;

    std::map<uint32_t, uint64_t> idDistribution() const;

private:
    mutable std::mutex m_mutex;
    uint64_t m_totalFrames;
    uint64_t m_totalBytes;
    uint64_t m_errorFrames;
    uint64_t m_remoteFrames;
    uint64_t m_startTimeUs;
    std::map<uint32_t, uint64_t> m_idCount;
};

#endif // CAN_STATS_H
