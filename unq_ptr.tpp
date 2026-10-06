#include "unq_ptr.h"
#include <stdexcept>

template <class T>
UnqPtr<T>::UnqPtr(void* object, T* ptr, void (*deleter)(void*)) noexcept
    : object(object), ptr(ptr), deleter(deleter), can_clone(true) {}

template <class T>
UnqPtr<T>::UnqPtr() noexcept
    : object(nullptr), ptr(nullptr), deleter(nullptr), can_clone(false) {}

template <class T>
UnqPtr<T>::UnqPtr(UnqPtr<T>&& other) noexcept
    : object(other.object), ptr(other.ptr), deleter(other.deleter), can_clone(other.can_clone) {
    other.object = nullptr;
    other.ptr = nullptr;
    other.deleter = nullptr;
    other.can_clone = false;
}

template <class T>
UnqPtr<T>& UnqPtr<T>::operator=(UnqPtr<T>&& other) noexcept {
    if (this == &other) return *this;

    reset();
    object = other.object;
    ptr = other.ptr;
    deleter = other.deleter;
    can_clone = other.can_clone;
    other.object = nullptr;
    other.ptr = nullptr;
    other.deleter = nullptr;
    other.can_clone = false;
    return *this;
}

template <class T>
template <class U, class>
UnqPtr<T>::UnqPtr(UnqPtr<U>&& other) noexcept
    : object(other.object), ptr(other.ptr), deleter(other.deleter), can_clone(false) {
    other.object = nullptr;
    other.ptr = nullptr;
    other.deleter = nullptr;
    other.can_clone = false;
}

template <class T>
template <class U, class>
UnqPtr<T>& UnqPtr<T>::operator=(UnqPtr<U>&& other) noexcept {
    UnqPtr<T> incoming(std::move(other));
    swap(incoming);
    return *this;
}

template <class T>
template <class... Args>
UnqPtr<T> UnqPtr<T>::make(Args&&... args) {
    using Stored = std::remove_cv_t<T>;
    auto* value = new Stored(std::forward<Args>(args)...);
    return UnqPtr<T>(value, value, [](void* object) { delete static_cast<Stored*>(object); });
}

template <class T>
UnqPtr<T> UnqPtr<T>::clone() const {
    if (object == nullptr) return UnqPtr<T>();
    if (!can_clone) throw std::logic_error("UnqPtr cannot be cloned after type conversion");

    if constexpr (std::is_constructible<std::remove_cv_t<T>, T&>::value) {
        return UnqPtr<T>::make(*ptr);
    } else {
        throw std::logic_error("Object type cannot be cloned");
    }
}

template <class T>
T* UnqPtr<T>::get() const noexcept {
    return ptr;
}

template <class T>
T& UnqPtr<T>::operator*() const {
    if (object == nullptr) throw std::logic_error("UnqPtr is empty");
    return *get();
}

template <class T>
T* UnqPtr<T>::operator->() const {
    if (object == nullptr) throw std::logic_error("UnqPtr is empty");
    return get();
}

template <class T>
UnqPtr<T>::operator bool() const noexcept {
    return object != nullptr;
}

template <class T>
void UnqPtr<T>::reset() noexcept {
    if (object == nullptr) return;

    void* old_object = object;
    auto old_deleter = deleter;
    object = nullptr;
    ptr = nullptr;
    deleter = nullptr;
    can_clone = false;
    old_deleter(old_object);
}

template <class T>
void UnqPtr<T>::swap(UnqPtr<T>& other) noexcept {
    std::swap(object, other.object);
    std::swap(ptr, other.ptr);
    std::swap(deleter, other.deleter);
    std::swap(can_clone, other.can_clone);
}

template <class T>
UnqPtr<T>::~UnqPtr() {
    reset();
}

template <class T>
UnqPtr<T[]>::UnqPtr(T* data, size_t length) noexcept : data(data), length(length) {}

template <class T>
UnqPtr<T[]>::UnqPtr() noexcept : data(nullptr), length(0) {}

template <class T>
UnqPtr<T[]>::UnqPtr(UnqPtr<T[]>&& other) noexcept
    : data(other.data), length(other.length) {
    other.data = nullptr;
    other.length = 0;
}

template <class T>
UnqPtr<T[]>& UnqPtr<T[]>::operator=(UnqPtr<T[]>&& other) noexcept {
    if (this == &other) return *this;

    reset();
    data = other.data;
    length = other.length;
    other.data = nullptr;
    other.length = 0;
    return *this;
}

template <class T>
UnqPtr<T[]> UnqPtr<T[]>::make_array(size_t count) {
    static_assert(std::is_default_constructible<T>::value,
                  "UnqPtr<T[]>::make_array() requires default-constructible elements");
    if (count == 0) return UnqPtr<T[]>();

    using Stored = std::remove_cv_t<T>;
    return UnqPtr<T[]>(new Stored[count](), count);
}

template <class T>
UnqPtr<T[]> UnqPtr<T[]>::clone() const {
    if (data == nullptr) return UnqPtr<T[]>();

    if constexpr (std::is_copy_assignable<T>::value) {
        auto copy = make_array(size());
        for (size_t index = 0; index < size(); index++) copy[index] = (*this)[index];
        return copy;
    } else {
        throw std::logic_error("Array element type cannot be cloned");
    }
}

template <class T>
T* UnqPtr<T[]>::get() const noexcept {
    return data;
}

template <class T>
T& UnqPtr<T[]>::operator[](size_t index) const {
    if (index >= size()) throw std::out_of_range("UnqPtr array index out of range");
    return get()[index];
}

template <class T>
UnqPtr<T[]>::operator bool() const noexcept {
    return data != nullptr;
}

template <class T>
size_t UnqPtr<T[]>::size() const noexcept {
    return length;
}

template <class T>
void UnqPtr<T[]>::reset() noexcept {
    T* old_data = data;
    data = nullptr;
    length = 0;
    delete[] old_data;
}

template <class T>
void UnqPtr<T[]>::swap(UnqPtr<T[]>& other) noexcept {
    std::swap(data, other.data);
    std::swap(length, other.length);
}

template <class T>
UnqPtr<T[]>::~UnqPtr() {
    reset();
}
