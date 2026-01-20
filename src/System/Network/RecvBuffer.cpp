#include "System/Network/RecvBuffer.h"
#include <cstring> // std::memmove

namespace System {

RecvBuffer::RecvBuffer(int32_t bufferSize) : _capacity(bufferSize), _readPos(0), _writePos(0)
{
    _buffer.resize(_capacity);
}

void RecvBuffer::Clean()
{
    int32_t dataSize = DataSize();

    if (dataSize == 0)
    {
        // [Fast Path] No data, just reset cursors
        _readPos = _writePos = 0;
        return;
    }

    // [Lazy Compaction] Only compact when absolutely necessary
    // This minimizes expensive memmove calls
    // 임계값(COMPACT_THRESHOLD, 10KB) 미만으로 여유 공간이 남았을 때만 압축을 수행하여
    // 빈번한 메모리 복사(memmove)로 인한 CPU 부하를 방지합니다.
    if (FreeSize() < COMPACT_THRESHOLD)
    {
        // [Slow Path] Move remaining data to front
        // std::memmove is safe for overlapping regions
        std::memmove(_buffer.data(), _buffer.data() + _readPos, dataSize);
        _readPos = 0;
        _writePos = dataSize;
    }
}

bool RecvBuffer::MoveReadPos(int32_t numOfBytes)
{
    if (numOfBytes > DataSize())
        return false;
    _readPos += numOfBytes;
    return true;
}

bool RecvBuffer::MoveWritePos(int32_t numOfBytes)
{
    if (numOfBytes > FreeSize())
        return false;
    _writePos += numOfBytes;
    return true;
}

} // namespace System
