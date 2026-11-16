#include "unq_ptr.h"
#include <stdexcept>

template <class T>
UnqPtr<T>::UnqPtr(T* ptr) noexcept : ptr(ptr) {}

template <class T>
T* UnqPtr<T>::release() noexcept {
    T* old = ptr;
    ptr = nullptr;

    return old;
}

template <class T>
UnqPtr<T>::UnqPtr() noexcept : ptr(nullptr) {}

template <class T>
UnqPtr<T>::UnqPtr(UnqPtr<T>&& other) noexcept : ptr(other.release()) {}

template <class T>
UnqPtr<T>& UnqPtr<T>::operator=(UnqPtr<T>&& other) noexcept {
    UnqPtr<T> incoming(std::move(other));
    swap(incoming);

    return *this;
}

template <class T>
template <class U, class>
UnqPtr<T>::UnqPtr(UnqPtr<U>&& other) noexcept : ptr(other.release()) {}

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
    return UnqPtr<T>(new std::remove_cv_t<T>(std::forward<Args>(args)...));
}

template <class T>
template <class U, class>
UnqPtr<T> UnqPtr<T>::clone() const {
    if (ptr == nullptr) return UnqPtr<T>();

    return UnqPtr<T>::make(*ptr);
}

template <class T>
T* UnqPtr<T>::get() const noexcept {
    return ptr;
}

template <class T>
T& UnqPtr<T>::operator*() const {
    if (ptr == nullptr) throw std::logic_error("UnqPtr is empty");

    return *ptr;
}

template <class T>
T* UnqPtr<T>::operator->() const {
    if (ptr == nullptr) throw std::logic_error("UnqPtr is empty");

    return ptr;
}

template <class T>
UnqPtr<T>::operator bool() const noexcept {
    return ptr != nullptr;
}

template <class T>
void UnqPtr<T>::reset() noexcept {
    delete release();
}

template <class T>
void UnqPtr<T>::swap(UnqPtr<T>& other) noexcept {
    std::swap(ptr, other.ptr);
}

template <class T>
UnqPtr<T>::~UnqPtr() {
    reset();
}

template <class T>
UnqPtr<T[]>::UnqPtr(T* data, size_t length) noexcept : data(data), length(length) {}

template <class T>
T* UnqPtr<T[]>::release() noexcept {
    T* old = data;
    data = nullptr;
    length = 0;

    return old;
}

template <class T>
UnqPtr<T[]>::UnqPtr() noexcept : data(nullptr), length(0) {}

template <class T>
UnqPtr<T[]>::UnqPtr(UnqPtr<T[]>&& other) noexcept : data(other.data), length(other.length) {
    other.data = nullptr;
    other.length = 0;
}

template <class T>
UnqPtr<T[]>& UnqPtr<T[]>::operator=(UnqPtr<T[]>&& other) noexcept {
    UnqPtr<T[]> incoming(std::move(other));
    swap(incoming);

    return *this;
}

template <class T>
UnqPtr<T[]> UnqPtr<T[]>::make_array(size_t count) {
    static_assert(std::is_default_constructible<T>::value, "UnqPtr<T[]>::make_array() requires default-constructible elements");
    if (count == 0) return UnqPtr<T[]>();

    return UnqPtr<T[]>(new std::remove_cv_t<T>[count](), count);
}

template <class T>
template <class U, class>
UnqPtr<T[]> UnqPtr<T[]>::clone() const {
    auto copy = make_array(length);
    for (size_t index = 0; index < length; index++) {
        copy.data[index] = data[index];
    }

    return copy;
}

template <class T>
T* UnqPtr<T[]>::get() const noexcept {
    return data;
}

template <class T>
T& UnqPtr<T[]>::operator[](size_t index) const {
    if (index >= length) throw std::out_of_range("UnqPtr array index out of range");

    return data[index];
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
    delete[] release();
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
