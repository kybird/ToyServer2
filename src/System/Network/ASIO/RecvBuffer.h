#pragma once

#include <cstdint>
#include <vector>

namespace System {

/*
    High-Performance Receive Buffer for MMORPG Servers.

    [Design]
    - Linear buffer with lazy compaction
    - Single IO thread access (no locks needed)
    - Tuned for typical MMORPG packet sizes (~100B - 4KB)

    [Hot Path Optimization]
    - OnWrite/OnRead: O(1), no branches in fast path
    - Clean(): Compaction only when free space is critically low (< 10KB)
    - std::memmove used for overlapping-safe copy
*/
class RecvBuffer
{
public:
    // [Tuning] Buffer size: 64KB is enough for most MMORPG scenarios
    // Larger buffers waste cache, smaller may cause frequent compaction
    static constexpr int32_t DEFAULT_CAPACITY = 64 * 1024; // 64KB

    // [Tuning] Compaction threshold: Only compact when free space < this
    // Higher = less compaction, but risk of buffer full
    static constexpr int32_t COMPACT_THRESHOLD = 10 * 1024; // 10KB

    RecvBuffer(int32_t bufferSize = DEFAULT_CAPACITY);
    ~RecvBuffer() = default;

    void Clean();
    bool OnRead(int32_t numOfBytes);
    bool OnWrite(int32_t numOfBytes);

    void Reset()
    {
        _readPos = _writePos = 0;
    }

    uint8_t *ReadPos()
    {
        return &_buffer[_readPos];
    }
    uint8_t *WritePos()
    {
        return &_buffer[_writePos];
    }

    int32_t DataSize() const
    {
        return _writePos - _readPos;
    }
    int32_t FreeSize() const
    {
        return _capacity - _writePos;
    }

private:
    int32_t _capacity = 0;
    int32_t _readPos = 0;
    int32_t _writePos = 0;
    std::vector<uint8_t> _buffer;
};

} // namespace System
