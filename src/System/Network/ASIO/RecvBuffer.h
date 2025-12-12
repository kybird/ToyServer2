#pragma once

#include <cstdint>
#include <vector>

namespace System {

/*
    Simple RingBuffer for Receive Logic.
    Not thread-safe (Should be accessed by IO thread only).
*/
class RecvBuffer {
  enum { BUFFER_COUNT = 10 };

public:
  RecvBuffer(int32_t bufferSize = 0x100000); // 1MB
  ~RecvBuffer() = default;

  void Clean();
  bool OnRead(int32_t numOfBytes);
  bool OnWrite(int32_t numOfBytes);

  void Reset() { _readPos = _writePos = 0; }

  uint8_t *ReadPos() { return &_buffer[_readPos]; }
  uint8_t *WritePos() { return &_buffer[_writePos]; }

  int32_t DataSize() { return _writePos - _readPos; }
  int32_t FreeSize() { return _capacity - _writePos; }

private:
  int32_t _capacity = 0;
  int32_t _readPos = 0;
  int32_t _writePos = 0;
  std::vector<uint8_t> _buffer;
};

} // namespace System
