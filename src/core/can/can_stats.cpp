#include "can/can_stats.h"
#include <chrono>

CanStats::CanStats()
    : m_totalFrames(0)
    , m_totalBytes(0)
    , m_errorFrames(0)
    , m_remoteFrames(0)
    , m_startTimeUs(0)
{
}

void CanStats::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_totalFrames = 0;
    m_totalBytes = 0;
    m_errorFrames = 0;
    m_remoteFrames = 0;
    m_startTimeUs = 0;
    m_idCount.clear();
}

void CanStats::update(const CanFrame& frame)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_startTimeUs == 0) {
        m_startTimeUs = frame.timestamp_us;
    }

    m_totalFrames++;
    m_totalBytes += frame.dlc;

    if (frame.error) m_errorFrames++;
    if (frame.remote) m_remoteFrames++;

    m_idCount[frame.id]++;
}

uint64_t CanStats::totalFrames() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalFrames;
}

uint64_t CanStats::totalBytes() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalBytes;
}

uint64_t CanStats::errorFrames() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_errorFrames;
}

uint64_t CanStats::remoteFrames() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_remoteFrames;
}

double CanStats::busLoadPercent(uint32_t bitrate_bps) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_startTimeUs == 0 || bitrate_bps == 0) return 0.0;

    uint64_t now = CanFrame::currentTimestampUs();
    double elapsedSec = (now - m_startTimeUs) / 1000000.0;
    if (elapsedSec <= 0) return 0.0;

    // 每帧开销：帧头+CRC+ACK等约47位（标准帧），扩展帧约67位
    double bitsPerFrame = 47 + 8 * 8; // 简化估算
    double totalBits = m_totalFrames * bitsPerFrame;
    double maxBits = bitrate_bps * elapsedSec;

    return (totalBits / maxBits) * 100.0;
}

uint32_t CanStats::uniqueIdCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<uint32_t>(m_idCount.size());
}

std::map<uint32_t, uint64_t> CanStats::idDistribution() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_idCount;
}
