#pragma once

#include "System/Dispatcher/IMessage.h"
#include "System/Dispatcher/MessagePool.h"
#include <algorithm> // for std::swap

namespace System {

/**
 * @brief PacketMessage를 위한 Intrusive Smart Pointer (Zero Overhead)
 *
 * std::shared_ptr와 달리 제어 블록(Control Block)을 할당하지 않으며,
 * IMessage 내부에 이미 존재하는 refCount를 직접 사용합니다.
 *
 * 주요 기능:
 * - RAII: 스코프 종료 시 자동으로 DecRef 수행
 * - Copy: 복사 시 AddRef 수행
 * - Move: 소유권 이전 (RefCount 변경 없음)
 * - Auto-Free: RefCount가 0이 되면 MessagePool::Free 호출
 */
class PacketPtr
{
public:
    // 1. 기본 생성자 (Empty)
    PacketPtr() : _ptr(nullptr)
    {
    }

    // 2. Raw Pointer에서 생성 (소유권 인수)
    // 주의: 이미 AddRef된 상태여야 함 (AllocatePacket 직후 등)
    // 만약 AddRef가 안 된 상태라면 별도로 처리해야 하지만, 보통 생성 시점에 1임.
    explicit PacketPtr(PacketMessage *ptr) : _ptr(ptr)
    {
        // AllocatePacket returns with refCount=1. We take ownership.
    }

    // 3. 복사 생성자 (AddRef)
    PacketPtr(const PacketPtr &other) : _ptr(other._ptr)
    {
        if (_ptr)
        {
            _ptr->AddRef();
        }
    }

    // 4. 이동 생성자 (소유권 이전)
    PacketPtr(PacketPtr &&other) noexcept : _ptr(other._ptr)
    {
        other._ptr = nullptr;
    }

    // 5. 소멸자 (DecRef & Free)
    ~PacketPtr()
    {
        Reset();
    }

    // 6. 복사 대입
    PacketPtr &operator=(const PacketPtr &other)
    {
        if (this != &other)
        {
            Reset(); // 기존 것 해제
            _ptr = other._ptr;
            if (_ptr)
            {
                _ptr->AddRef();
            }
        }
        return *this;
    }

    // 7. 이동 대입
    PacketPtr &operator=(PacketPtr &&other) noexcept
    {
        if (this != &other)
        {
            Reset(); // 기존 것 해제
            _ptr = other._ptr;
            other._ptr = nullptr;
        }
        return *this;
    }

    // 8. 헬퍼 메서드
    void Reset()
    {
        if (_ptr)
        {
            MessagePool::Free(_ptr);
            _ptr = nullptr;
        }
    }

    PacketMessage *Release()
    {
        PacketMessage *temp = _ptr;
        _ptr = nullptr;
        return temp;
    }

    PacketMessage *Get() const
    {
        return _ptr;
    }

    PacketMessage *operator->() const
    {
        return _ptr;
    }

    PacketMessage &operator*() const
    {
        return *_ptr;
    }

    explicit operator bool() const
    {
        return _ptr != nullptr;
    }

private:
    PacketMessage *_ptr;
};

} // namespace System
