#include "shrd_ptr.h"
#include <stdexcept>

template <class T>
ShrdPtr<T>::ShrdPtr() noexcept : record(nullptr), ptr(nullptr) {}

template <class T>
ShrdPtr<T>::ShrdPtr(const ShrdPtr<T>& other) noexcept : record(other.record), ptr(other.ptr) {
    if (record != nullptr) record->descriptors++;
}

template <class T>
ShrdPtr<T>::ShrdPtr(ShrdPtr<T>&& other) noexcept : record(other.record), ptr(other.ptr) {
    other.record = nullptr;
    other.ptr = nullptr;
}

template <class T>
template <class U, class>
ShrdPtr<T>::ShrdPtr(const UnqPtr<U>& owner) noexcept : record(owner.record), ptr(owner.get()) {
    if (record != nullptr) record->descriptors++;
}

template <class T>
template <class U, class>
ShrdPtr<T>::ShrdPtr(const ShrdPtr<U>& other) noexcept : record(other.record), ptr(other.ptr) {
    if (record != nullptr) record->descriptors++;
}

template <class T>
template <class U, class>
ShrdPtr<T>::ShrdPtr(ShrdPtr<U>&& other) noexcept : record(other.record), ptr(other.ptr) {
    other.record = nullptr;
    other.ptr = nullptr;
}

template <class T>
ShrdPtr<T>& ShrdPtr<T>::operator=(const ShrdPtr<T>& other) noexcept {
    if (this == &other) return *this;

    ShrdPtr<T> incoming(other);
    swap(incoming);
    return *this;
}

template <class T>
ShrdPtr<T>& ShrdPtr<T>::operator=(ShrdPtr<T>&& other) noexcept {
    if (this == &other) return *this;

    ShrdPtr<T> incoming(std::move(other));
    swap(incoming);
    return *this;
}

template <class T>
template <class U, class>
ShrdPtr<T>& ShrdPtr<T>::operator=(const ShrdPtr<U>& other) noexcept {
    ShrdPtr<T> incoming(other);
    swap(incoming);
    return *this;
}

template <class T>
template <class U, class>
ShrdPtr<T>& ShrdPtr<T>::operator=(ShrdPtr<U>&& other) noexcept {
    ShrdPtr<T> incoming(std::move(other));
    swap(incoming);
    return *this;
}

template <class T>
T* ShrdPtr<T>::get() const noexcept {
    return ptr;
}

template <class T>
T& ShrdPtr<T>::operator*() const {
    if (ptr == nullptr) throw std::logic_error("ShrdPtr is empty");
    return *ptr;
}

template <class T>
T* ShrdPtr<T>::operator->() const {
    if (ptr == nullptr) throw std::logic_error("ShrdPtr is empty");
    return ptr;
}

template <class T>
ShrdPtr<T>::operator bool() const noexcept {
    return ptr != nullptr;
}

template <class T>
size_t ShrdPtr<T>::use_count() const noexcept {
    return record == nullptr ? 0 : record->descriptors;
}

template <class T>
void ShrdPtr<T>::reset() noexcept {
    if (record == nullptr) return;

    auto* old_record = record;
    record = nullptr;
    ptr = nullptr;
    old_record->descriptors--;
    if (old_record->descriptors == 0 && !old_record->has_owner) delete old_record;
}

template <class T>
void ShrdPtr<T>::swap(ShrdPtr<T>& other) noexcept {
    std::swap(record, other.record);
    std::swap(ptr, other.ptr);
}

template <class T>
ShrdPtr<T>::~ShrdPtr() {
    reset();
}

template <class T>
ShrdPtr<T[]>::ShrdPtr() noexcept : record(nullptr) {}

template <class T>
ShrdPtr<T[]>::ShrdPtr(const UnqPtr<T[]>& owner) noexcept
    : record(owner.record) {
    if (record != nullptr) record->descriptors++;
}

template <class T>
ShrdPtr<T[]>::ShrdPtr(const ShrdPtr<T[]>& other) noexcept
    : record(other.record) {
    if (record != nullptr) record->descriptors++;
}

template <class T>
ShrdPtr<T[]>::ShrdPtr(ShrdPtr<T[]>&& other) noexcept
    : record(other.record) {
    other.record = nullptr;
}

template <class T>
ShrdPtr<T[]>& ShrdPtr<T[]>::operator=(const ShrdPtr<T[]>& other) noexcept {
    if (this == &other) return *this;

    ShrdPtr<T[]> incoming(other);
    swap(incoming);
    return *this;
}

template <class T>
ShrdPtr<T[]>& ShrdPtr<T[]>::operator=(ShrdPtr<T[]>&& other) noexcept {
    if (this == &other) return *this;

    ShrdPtr<T[]> incoming(std::move(other));
    swap(incoming);
    return *this;
}

template <class T>
T* ShrdPtr<T[]>::get() const noexcept {
    return record == nullptr ? nullptr : record->data;
}

template <class T>
T& ShrdPtr<T[]>::operator[](size_t index) const {
    if (index >= size()) throw std::out_of_range("ShrdPtr array index out of range");
    return record->data[index];
}

template <class T>
ShrdPtr<T[]>::operator bool() const noexcept {
    return record != nullptr;
}

template <class T>
size_t ShrdPtr<T[]>::size() const noexcept {
    return record == nullptr ? 0 : record->count;
}

template <class T>
size_t ShrdPtr<T[]>::use_count() const noexcept {
    return record == nullptr ? 0 : record->descriptors;
}

template <class T>
void ShrdPtr<T[]>::reset() noexcept {
    if (record == nullptr) return;

    auto* old_record = record;
    record = nullptr;
    old_record->descriptors--;
    if (old_record->descriptors == 0 && !old_record->has_owner) delete old_record;
}

template <class T>
void ShrdPtr<T[]>::swap(ShrdPtr<T[]>& other) noexcept {
    std::swap(record, other.record);
}

template <class T>
ShrdPtr<T[]>::~ShrdPtr() {
    reset();
}
