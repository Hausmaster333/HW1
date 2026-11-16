#ifndef WEAK_PTR_H
#define WEAK_PTR_H

#include <cstddef>
#include <type_traits>
#include "shrd_ptr.h"

template <class T>
class WeakPtr {
    static_assert(!std::is_array<T>::value, "WeakPtr<T[]> is not supported");

    private:
        ControlBlock* block;
        T* ptr;
    public:
        WeakPtr() noexcept;
        WeakPtr(const WeakPtr<T>& other) noexcept;
        WeakPtr(WeakPtr<T>&& other) noexcept;

        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        WeakPtr(const ShrdPtr<U>& owner) noexcept;

        WeakPtr<T>& operator=(const WeakPtr<T>& other) noexcept;
        WeakPtr<T>& operator=(WeakPtr<T>&& other) noexcept;

        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        WeakPtr<T>& operator=(const ShrdPtr<U>& owner) noexcept;

        ShrdPtr<T> lock() const noexcept;

        bool expired() const noexcept;
        size_t use_count() const noexcept;
        void reset() noexcept;
        void swap(WeakPtr<T>& other) noexcept;

        ~WeakPtr();
};

#include "weak_ptr.tpp"

#endif
