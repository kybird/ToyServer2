#pragma once
#include <type_traits>
#include <utility>

namespace System {

/**
 * @brief Custom smart pointer that replaces boost::intrusive_ptr.
 *
 * Requirements:
 * 1. Supports Default, Raw, Copy, and Move constructors/assignments.
 * 2. Move semantic avoids incrementing/decrementing m_refCount (Pointer Swap only).
 * 3. Essential utilities (get, ->, *, bool, ==).
 */
template <typename T> class RefPtr
{
public:
    using element_type = T;

    // Default Constructor
    RefPtr() noexcept : _ptr(nullptr)
    {
    }

    // 2. nullptr constructor
    constexpr RefPtr(std::nullptr_t) noexcept : _ptr(nullptr)
    {
    }

    // Construct from raw pointer (assumes AddRef is needed if newly acquiring)
    explicit RefPtr(T *ptr) noexcept : _ptr(ptr)
    {
        if (_ptr)
            _ptr->AddRef();
    }

    // Copy Constructor
    RefPtr(const RefPtr &other) noexcept : _ptr(other._ptr)
    {
        if (_ptr)
            _ptr->AddRef();
    }

    // Move Constructor
    RefPtr(RefPtr &&other) noexcept : _ptr(other._ptr)
    {
        other._ptr = nullptr; // Steal the pointer
    }

    // Advanced: Template copy constructor for inheritance (e.g. RefPtr<Base> = RefPtr<Derived>)
    template <typename U> RefPtr(const RefPtr<U> &rhs) : _ptr(rhs.get())
    {
        if (_ptr)
        {
            _ptr->AddRef();
        }
    }

    // Advanced: Template move constructor
    template <typename U> RefPtr(RefPtr<U> &&rhs) noexcept : _ptr(rhs.get())
    {
        rhs.detach();
    }

    // Destructor
    ~RefPtr() noexcept
    {
        if (_ptr)
            _ptr->Release();
    }

    // Copy Assignment
    RefPtr &operator=(const RefPtr &other) noexcept
    {
        // Copy and Swap idiom: guarantees strong exception safety and handles self-assignment safely
        RefPtr(other).swap(*this);
        return *this;
    }

    // Move Assignment
    RefPtr &operator=(RefPtr &&other) noexcept
    {
        // Move and Swap idiom
        RefPtr(std::move(other)).swap(*this);
        return *this;
    }

    // Assign from raw pointer
    RefPtr &operator=(T *ptr) noexcept
    {
        RefPtr(ptr).swap(*this);
        return *this;
    }

    // Utility Operators
    T *get() const noexcept
    {
        return _ptr;
    }

    T &operator*() const noexcept
    {
        return *_ptr;
    }

    T *operator->() const noexcept
    {
        return _ptr;
    }

    explicit operator bool() const noexcept
    {
        return _ptr != nullptr;
    }

    void swap(RefPtr &rhs) noexcept
    {
        std::swap(_ptr, rhs._ptr);
    }

    // Construct from raw pointer with optional AddRef
    RefPtr(T *ptr, bool add_ref) noexcept : _ptr(ptr)
    {
        if (_ptr && add_ref)
            _ptr->AddRef();
    }

    void reset() noexcept
    {
        RefPtr().swap(*this);
    }

    void reset(T *rhs, bool add_ref = true)
    {
        RefPtr(rhs, add_ref).swap(*this);
    }

    // Detach ownership without releasing the refcount
    T *detach() noexcept
    {
        T *ret = _ptr;
        _ptr = nullptr;
        return ret;
    }

private:
    T *_ptr;
};

// Equality Operators
template <class T, class U> inline bool operator==(const RefPtr<T> &a, const RefPtr<U> &b) noexcept
{
    return a.get() == b.get();
}

template <class T, class U> inline bool operator!=(const RefPtr<T> &a, const RefPtr<U> &b) noexcept
{
    return a.get() != b.get();
}

template <class T> inline bool operator==(const RefPtr<T> &a, std::nullptr_t) noexcept
{
    return a.get() == nullptr;
}

template <class T> inline bool operator==(std::nullptr_t, const RefPtr<T> &b) noexcept
{
    return nullptr == b.get();
}

template <class T> inline bool operator!=(const RefPtr<T> &a, std::nullptr_t) noexcept
{
    return a.get() != nullptr;
}

template <class T> inline bool operator!=(std::nullptr_t, const RefPtr<T> &b) noexcept
{
    return nullptr != b.get();
}

} // namespace System
