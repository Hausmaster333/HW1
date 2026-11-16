#ifndef UNQ_PTR_H
#define UNQ_PTR_H

#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>

// Владеть From через To* можно, если delete To* вызовет деструктор настоящего типа From
template <class From, class To>
inline constexpr bool is_owner_convertible =
    std::is_convertible<From*, To*>::value &&
    (std::is_same<std::remove_cv_t<From>, std::remove_cv_t<To>>::value || std::has_virtual_destructor<To>::value);

// Копия через конструктор T не срежет объект, только если за T* не может прятаться наследник
template <class T>
inline constexpr bool is_cloneable =
    std::is_copy_constructible<std::remove_cv_t<T>>::value &&
    (!std::is_polymorphic<T>::value || std::is_final<T>::value);

template <class T>
class UnqPtr {
    static_assert(!std::is_array<T>::value, "Use UnqPtr<T[]> for arrays");

    template <class U> friend class UnqPtr;
    template <class U> friend class ShrdPtr;
    private:
        T* ptr;

        explicit UnqPtr(T* ptr) noexcept;
        T* release() noexcept;
    public:
        UnqPtr() noexcept;

        UnqPtr(const UnqPtr<T>& other) = delete;
        UnqPtr<T>& operator=(const UnqPtr<T>& other) = delete;

        UnqPtr(UnqPtr<T>&& other) noexcept;
        UnqPtr<T>& operator=(UnqPtr<T>&& other) noexcept;

        template <class U, class = std::enable_if_t<is_owner_convertible<U, T>>>
        UnqPtr(UnqPtr<U>&& other) noexcept;

        template <class U, class = std::enable_if_t<is_owner_convertible<U, T>>>
        UnqPtr<T>& operator=(UnqPtr<U>&& other) noexcept;

        template <class... Args>
        static UnqPtr<T> make(Args&&... args);

        template <class U = T, class = std::enable_if_t<is_cloneable<U>>>
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
        T* release() noexcept;
    public:
        UnqPtr() noexcept;

        UnqPtr(const UnqPtr<T[]>& other) = delete;
        UnqPtr<T[]>& operator=(const UnqPtr<T[]>& other) = delete;

        UnqPtr(UnqPtr<T[]>&& other) noexcept;
        UnqPtr<T[]>& operator=(UnqPtr<T[]>&& other) noexcept;

        static UnqPtr<T[]> make_array(size_t count);

        template <class U = T, class = std::enable_if_t<std::is_copy_assignable<U>::value>>
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
