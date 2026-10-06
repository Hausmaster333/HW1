#include "shrd_ptr.h"
#include <stdexcept>

template <class T>
ShrdPtr<T>::ShrdPtr(decentral_detail::ControlBlock* block, T* ptr) noexcept
    : block(block), ptr(ptr) {}

template <class T>
ShrdPtr<T>::ShrdPtr() noexcept : block(nullptr), ptr(nullptr) {}

template <class T>
ShrdPtr<T>::ShrdPtr(const ShrdPtr<T>& other) noexcept : block(other.block), ptr(other.ptr) {
    if (block != nullptr) block->references++;
}

template <class T>
ShrdPtr<T>::ShrdPtr(ShrdPtr<T>&& other) noexcept : block(other.block), ptr(other.ptr) {
    other.block = nullptr;
    other.ptr = nullptr;
}

template <class T>
template <class U, class>
ShrdPtr<T>::ShrdPtr(const ShrdPtr<U>& other) noexcept : block(other.block), ptr(other.ptr) {
    if (block != nullptr) block->references++;
}

template <class T>
template <class U, class>
ShrdPtr<T>::ShrdPtr(ShrdPtr<U>&& other) noexcept : block(other.block), ptr(other.ptr) {
    other.block = nullptr;
    other.ptr = nullptr;
}

template <class T>
template <class U, class>
ShrdPtr<T>::ShrdPtr(UnqPtr<U>&& other) : block(nullptr), ptr(nullptr) {
    if (other.object == nullptr) return;

    auto* new_block = new decentral_detail::ControlBlock(other.deleter);
    new_block->object = other.object;
    block = new_block;
    ptr = other.ptr;
    other.object = nullptr;
    other.ptr = nullptr;
    other.deleter = nullptr;
    other.can_clone = false;
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
template <class... Args>
ShrdPtr<T> ShrdPtr<T>::make(Args&&... args) {
    using Stored = std::remove_cv_t<T>;
    auto* new_block = new decentral_detail::ControlBlock(
        [](void* object) { delete static_cast<Stored*>(object); });
    try {
        new_block->object = new Stored(std::forward<Args>(args)...);
    } catch (...) {
        delete new_block;
        throw;
    }
    return ShrdPtr<T>(new_block, static_cast<Stored*>(new_block->object));
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
    return block == nullptr ? 0 : block->references;
}

template <class T>
void ShrdPtr<T>::reset() noexcept {
    if (block == nullptr) return;

    auto* old_block = block;
    block = nullptr;
    ptr = nullptr;
    old_block->references--;
    if (old_block->references == 0) delete old_block;
}

template <class T>
void ShrdPtr<T>::swap(ShrdPtr<T>& other) noexcept {
    std::swap(block, other.block);
    std::swap(ptr, other.ptr);
}

template <class T>
ShrdPtr<T>::~ShrdPtr() {
    reset();
}

template <class T>
ShrdPtr<T[]>::ShrdPtr(decentral_detail::ControlBlock* block) noexcept : block(block) {}

template <class T>
ShrdPtr<T[]>::ShrdPtr() noexcept : block(nullptr) {}

template <class T>
ShrdPtr<T[]>::ShrdPtr(const ShrdPtr<T[]>& other) noexcept
    : block(other.block) {
    if (block != nullptr) block->references++;
}

template <class T>
ShrdPtr<T[]>::ShrdPtr(ShrdPtr<T[]>&& other) noexcept
    : block(other.block) {
    other.block = nullptr;
}

template <class T>
ShrdPtr<T[]>::ShrdPtr(UnqPtr<T[]>&& other) : block(nullptr) {
    if (other.data == nullptr) return;

    using Stored = std::remove_cv_t<T>;
    auto* new_block = new decentral_detail::ControlBlock(
        [](void* object) { delete[] static_cast<Stored*>(object); }, other.length);
    new_block->object = const_cast<Stored*>(other.data);
    block = new_block;
    other.data = nullptr;
    other.length = 0;
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
ShrdPtr<T[]> ShrdPtr<T[]>::make_array(size_t count) {
    static_assert(std::is_default_constructible<T>::value,
                  "ShrdPtr<T[]>::make_array() requires default-constructible elements");
    if (count == 0) return ShrdPtr<T[]>();

    using Stored = std::remove_cv_t<T>;
    auto* new_block = new decentral_detail::ControlBlock(
        [](void* object) { delete[] static_cast<Stored*>(object); }, count);
    try {
        new_block->object = new Stored[count]();
    } catch (...) {
        delete new_block;
        throw;
    }
    return ShrdPtr<T[]>(new_block);
}

template <class T>
T* ShrdPtr<T[]>::get() const noexcept {
    return block == nullptr ? nullptr : static_cast<T*>(block->object);
}

template <class T>
T& ShrdPtr<T[]>::operator[](size_t index) const {
    if (index >= size()) throw std::out_of_range("ShrdPtr array index out of range");
    return get()[index];
}

template <class T>
ShrdPtr<T[]>::operator bool() const noexcept {
    return block != nullptr;
}

template <class T>
size_t ShrdPtr<T[]>::size() const noexcept {
    return block == nullptr ? 0 : block->length;
}

template <class T>
size_t ShrdPtr<T[]>::use_count() const noexcept {
    return block == nullptr ? 0 : block->references;
}

template <class T>
void ShrdPtr<T[]>::reset() noexcept {
    if (block == nullptr) return;

    auto* old_block = block;
    block = nullptr;
    old_block->references--;
    if (old_block->references == 0) delete old_block;
}

template <class T>
void ShrdPtr<T[]>::swap(ShrdPtr<T[]>& other) noexcept {
    std::swap(block, other.block);
}

template <class T>
ShrdPtr<T[]>::~ShrdPtr() {
    reset();
}
