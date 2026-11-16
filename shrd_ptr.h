#ifndef SHRD_PTR_H
#define SHRD_PTR_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include "unq_ptr.h"

struct ControlBlock {
    std::atomic<std::uint32_t> references{1};
    std::atomic<std::uint32_t> weak_references{1}; // WeakPtr плюс одна общая доля всех владельцев

    ControlBlock() = default;
    ControlBlock(const ControlBlock&) = delete;
    ControlBlock& operator=(const ControlBlock&) = delete;
    virtual void destroy_object() noexcept = 0;
    
    virtual ~ControlBlock() = default;
};

template <class U>
struct InplaceBlock final : ControlBlock {
    alignas(U) unsigned char storage[sizeof(U)];

    template <class... Args>
    explicit InplaceBlock(Args&&... args) {
        ::new (static_cast<void*>(storage)) U(std::forward<Args>(args)...);
    }

    U* object() noexcept { return std::launder(reinterpret_cast<U*>(storage)); }
    void destroy_object() noexcept override { object()->~U(); }
};

template <class U> // Блок для объекта, созданного раньше (передача из UnqPtr)
struct PointerBlock final : ControlBlock {
    U* object;

    explicit PointerBlock(U* object) noexcept : object(object) {}
    void destroy_object() noexcept override { delete object; }
};

template <class U> // Блок для массива, созданного через new[]
struct ArrayBlock final : ControlBlock {
    U* data;

    explicit ArrayBlock(U* data) noexcept : data(data) {}
    void destroy_object() noexcept override { delete[] data; }
};

template <class T>
class ShrdPtr {
    static_assert(!std::is_array<T>::value, "Use ShrdPtr<T[]> for arrays");

    template <class U> friend class ShrdPtr;
    template <class U> friend class WeakPtr;

    template <class To, class From>
    friend ShrdPtr<To> static_pointer_cast(const ShrdPtr<From>& source) noexcept;
    private:
        ControlBlock* block;
        T* ptr;

        ShrdPtr(ControlBlock* block, T* ptr) noexcept;
    public:
        ShrdPtr() noexcept;
        ShrdPtr(const ShrdPtr<T>& other) noexcept;
        ShrdPtr(ShrdPtr<T>&& other) noexcept;

        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        ShrdPtr(const ShrdPtr<U>& other) noexcept;

        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        ShrdPtr(ShrdPtr<U>&& other) noexcept;

        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        ShrdPtr(UnqPtr<U>&& other);

        ShrdPtr<T>& operator=(const ShrdPtr<T>& other) noexcept;
        ShrdPtr<T>& operator=(ShrdPtr<T>&& other) noexcept;

        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        ShrdPtr<T>& operator=(const ShrdPtr<U>& other) noexcept;

        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        ShrdPtr<T>& operator=(ShrdPtr<U>&& other) noexcept;

        template <class... Args>
        static ShrdPtr<T> make(Args&&... args);

        T* get() const noexcept;
        T& operator*() const;
        T* operator->() const;
        explicit operator bool() const noexcept;
       
        size_t use_count() const noexcept;

        template <class U>
        bool operator==(const ShrdPtr<U>& other) const noexcept;
        template <class U>
        bool operator!=(const ShrdPtr<U>& other) const noexcept;

        void reset() noexcept;
        void swap(ShrdPtr<T>& other) noexcept;

        ~ShrdPtr();
};

template <class To, class From>
ShrdPtr<To> static_pointer_cast(const ShrdPtr<From>& source) noexcept;

template <class T>
class ShrdPtr<T[]> {
    private:
        ControlBlock* block;
        T* data;
        size_t length;

        ShrdPtr(ControlBlock* block, T* data, size_t length) noexcept;
    public:
        ShrdPtr() noexcept;
        ShrdPtr(const ShrdPtr<T[]>& other) noexcept;
        ShrdPtr(ShrdPtr<T[]>&& other) noexcept;

        ShrdPtr(UnqPtr<T[]>&& other);

        ShrdPtr<T[]>& operator=(const ShrdPtr<T[]>& other) noexcept;
        ShrdPtr<T[]>& operator=(ShrdPtr<T[]>&& other) noexcept;

        static ShrdPtr<T[]> make_array(size_t count);

        T* get() const noexcept;
        T& operator[](size_t index) const;
        explicit operator bool() const noexcept;
       
        size_t size() const noexcept;
        size_t use_count() const noexcept;
        void reset() noexcept;
        void swap(ShrdPtr<T[]>& other) noexcept;

        ~ShrdPtr();
};

#include "shrd_ptr.tpp"

#endif
