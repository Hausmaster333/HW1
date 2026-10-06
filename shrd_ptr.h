#ifndef SHRD_PTR_H
#define SHRD_PTR_H

#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include "unq_ptr.h"

namespace decentral_detail {
    struct ControlBlock {
        void* object = nullptr;
        void (*deleter)(void*);
        size_t references = 1;
        size_t length;

        ControlBlock(void (*deleter)(void*), size_t length = 0) noexcept
            : deleter(deleter), length(length) {}
        ControlBlock(const ControlBlock&) = delete;
        ControlBlock& operator=(const ControlBlock&) = delete;
        ~ControlBlock() { deleter(object); }
    };
}

template <class T>
class ShrdPtr {
    static_assert(!std::is_array<T>::value, "Use ShrdPtr<T[]> for arrays");

    template <class U> friend class ShrdPtr;
    private:
        decentral_detail::ControlBlock* block;
        T* ptr;

        ShrdPtr(decentral_detail::ControlBlock* block, T* ptr) noexcept;
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

        void reset() noexcept;
        void swap(ShrdPtr<T>& other) noexcept;
        ~ShrdPtr();
};

template <class T>
class ShrdPtr<T[]> {
    private:
        decentral_detail::ControlBlock* block;

        explicit ShrdPtr(decentral_detail::ControlBlock* block) noexcept;
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
