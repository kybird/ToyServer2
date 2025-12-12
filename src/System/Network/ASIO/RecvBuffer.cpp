#include "System/Network/ASIO/RecvBuffer.h"

namespace System {

RecvBuffer::RecvBuffer(int32_t bufferSize)
    : _capacity(bufferSize), _readPos(0), _writePos(0) {
  _buffer.resize(_capacity);
}

void RecvBuffer::Clean() {
  int32_t dataSize = DataSize();
  if (dataSize == 0) {
    // Empty, reset cursors
    _readPos = _writePos = 0;
  } else {
    // Move remaining data to front if free space is low
    // Or conceptually, usually we move when FreeSize < 1 packet max size
    if (FreeSize() < _capacity / BUFFER_COUNT) {
      std::copy(_buffer.begin() + _readPos, _buffer.begin() + _writePos,
                _buffer.begin());
      _readPos = 0;
      _writePos = dataSize;
    }
  }
}

bool RecvBuffer::OnRead(int32_t numOfBytes) {
  if (numOfBytes > DataSize())
    return false;
  _readPos += numOfBytes;
  return true;
}

bool RecvBuffer::OnWrite(int32_t numOfBytes) {
  if (numOfBytes > FreeSize())
    return false;
  _writePos += numOfBytes;
  return true;
}

} // namespace System
