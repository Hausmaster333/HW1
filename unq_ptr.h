#ifndef UNQ_PTR_H
#define UNQ_PTR_H

#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <class T> class ShrdPtr;

template <class T>
class UnqPtr {
    static_assert(!std::is_array<T>::value, "Use UnqPtr<T[]> for arrays");

    template <class U> friend class UnqPtr;
    template <class U> friend class ShrdPtr;
    private:
        void* object;
        T* ptr;
        void (*deleter)(void*);
        bool can_clone;

        UnqPtr(void* object, T* ptr, void (*deleter)(void*)) noexcept;
    public:
        UnqPtr() noexcept;
        UnqPtr(const UnqPtr<T>& other) = delete;
        UnqPtr<T>& operator=(const UnqPtr<T>& other) = delete;
        UnqPtr(UnqPtr<T>&& other) noexcept;
        UnqPtr<T>& operator=(UnqPtr<T>&& other) noexcept;

        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        UnqPtr(UnqPtr<U>&& other) noexcept;

        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        UnqPtr<T>& operator=(UnqPtr<U>&& other) noexcept;

        template <class... Args>
        static UnqPtr<T> make(Args&&... args);

        UnqPtr<T> clone() const;

        T* get() const noexcept;
        T& operator*() const;
        T* operator->() const;
        explicit operator bool() const noexcept;

        void reset() noexcept;
        void swap(UnqPtr<T>& other) noexcept;
        ~UnqPtr();
};

template <class T>
class UnqPtr<T[]> {
    template <class U> friend class ShrdPtr;
    private:
        T* data;
        size_t length;

        UnqPtr(T* data, size_t length) noexcept;
    public:
        UnqPtr() noexcept;
        UnqPtr(const UnqPtr<T[]>& other) = delete;
        UnqPtr<T[]>& operator=(const UnqPtr<T[]>& other) = delete;
        UnqPtr(UnqPtr<T[]>&& other) noexcept;
        UnqPtr<T[]>& operator=(UnqPtr<T[]>&& other) noexcept;

        static UnqPtr<T[]> make_array(size_t count);
        UnqPtr<T[]> clone() const;

        T* get() const noexcept;
        T& operator[](size_t index) const;
        explicit operator bool() const noexcept;
        size_t size() const noexcept;

        void reset() noexcept;
        void swap(UnqPtr<T[]>& other) noexcept;
        ~UnqPtr();
};

#include "unq_ptr.tpp"

#endif
