#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>


namespace System {

/*
    High-Performance Binary Buffer Helper.
    Designed for packet serialization/deserialization.

    Optimization Notes:
    - Methods are inline for hot-path optimization.
    - Uses std::vector for memory management but allows reservation.
    - Read operations throw std::underflow_error if safety is needed,
      or use CanRead() for manual checks.
*/
class ByteBuffer
{
public:
    ByteBuffer() : _readPos(0), _writePos(0)
    {
        _buffer.reserve(4096); // Default reservation to avoid frequent reallocs
    }

    ByteBuffer(size_t reserveSize) : _readPos(0), _writePos(0)
    {
        _buffer.reserve(reserveSize);
    }

    // Writer Methods
    template <typename T> void Write(const T &value)
    {
        static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
        Append((const uint8_t *)&value, sizeof(T));
    }

    void Write(const std::string &value)
    {
        uint16_t len = static_cast<uint16_t>(value.length());
        Write(len);
        Append((const uint8_t *)value.c_str(), len);
    }

    void WriteBytes(const uint8_t *src, size_t len)
    {
        Append(src, len);
    }

    // Reader Methods
    template <typename T> T Read()
    {
        static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
        if (_readPos + sizeof(T) > _writePos)
            throw std::underflow_error("ByteBuffer Read Error");

        T value;
        std::memcpy(&value, &_buffer[_readPos], sizeof(T));
        _readPos += sizeof(T);
        return value;
    }

    void Read(std::string &outValue)
    {
        uint16_t len = Read<uint16_t>();
        if (_readPos + len > _writePos)
            throw std::underflow_error("ByteBuffer Read Error");

        outValue.assign((const char *)&_buffer[_readPos], len);
        _readPos += len;
    }

    // Utility
    void Clear()
    {
        _readPos = 0;
        _writePos = 0;
        _buffer.clear();
    }
    size_t Size() const
    {
        return _writePos;
    }
    size_t Remaining() const
    {
        return _writePos - _readPos;
    }
    const uint8_t *Data() const
    {
        return _buffer.data();
    }

private:
    void Append(const uint8_t *src, size_t len)
    {
        if (_buffer.size() < _writePos + len)
            _buffer.resize(_writePos + len);

        std::memcpy(&_buffer[_writePos], src, len);
        _writePos += len;
    }

    std::vector<uint8_t> _buffer;
    size_t _readPos;
    size_t _writePos;
};

} // namespace System
