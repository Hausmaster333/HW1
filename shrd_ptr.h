#ifndef SHRD_PTR_H
#define SHRD_PTR_H

#include "unq_ptr.h"
#include <cstddef>
#include <type_traits>
#include <utility>

template <class T>
class ShrdPtr {
    static_assert(!std::is_array<T>::value, "Use ShrdPtr<T[]> for arrays");

    template <class U> friend class ShrdPtr;
    private:
        decentral_detail::Record* record;
        T* ptr; // Невладеющий адрес; временем жизни управляет record.
    public:
        ShrdPtr() noexcept;
        ShrdPtr(const ShrdPtr<T>& other) noexcept;
        ShrdPtr(ShrdPtr<T>&& other) noexcept;

        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        explicit ShrdPtr(const UnqPtr<U>& owner) noexcept;

        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        ShrdPtr(const ShrdPtr<U>& other) noexcept;

        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        ShrdPtr(ShrdPtr<U>&& other) noexcept;

        ShrdPtr<T>& operator=(const ShrdPtr<T>& other) noexcept;
        ShrdPtr<T>& operator=(ShrdPtr<T>&& other) noexcept;

        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        ShrdPtr<T>& operator=(const ShrdPtr<U>& other) noexcept;

        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        ShrdPtr<T>& operator=(ShrdPtr<U>&& other) noexcept;

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
        decentral_detail::ArrayRecord<T>* record;
    public:
        ShrdPtr() noexcept;
        explicit ShrdPtr(const UnqPtr<T[]>& owner) noexcept;
        ShrdPtr(const ShrdPtr<T[]>& other) noexcept;
        ShrdPtr(ShrdPtr<T[]>&& other) noexcept;
        ShrdPtr<T[]>& operator=(const ShrdPtr<T[]>& other) noexcept;
        ShrdPtr<T[]>& operator=(ShrdPtr<T[]>&& other) noexcept;

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
