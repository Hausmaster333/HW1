#include "shrd_ptr.h"
#include <stdexcept>

inline void acquire_block(ControlBlock* block) noexcept {
    if (block != nullptr) block->references.fetch_add(1, std::memory_order_relaxed);
}

inline void release_weak(ControlBlock* block) noexcept {
    if (block->weak_references.fetch_sub(1, std::memory_order_acq_rel) == 1) delete block;
}

inline void release_block(ControlBlock* block) noexcept {
    if (block == nullptr) return;

    if (block->references.load(std::memory_order_acquire) == 1 &&
        block->weak_references.load(std::memory_order_acquire) == 1) {
        block->destroy_object();
        delete block;
        return;
    }

    if (block->references.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        block->destroy_object();
        release_weak(block);
    }
}

inline size_t count_owners(const ControlBlock* block) noexcept {
    return block == nullptr ? 0 : block->references.load(std::memory_order_relaxed);
}

template <class T>
ShrdPtr<T>::ShrdPtr(ControlBlock* block, T* ptr) noexcept : block(block), ptr(ptr) {}

template <class T>
ShrdPtr<T>::ShrdPtr() noexcept : block(nullptr), ptr(nullptr) {}

template <class T>
ShrdPtr<T>::ShrdPtr(const ShrdPtr<T>& other) noexcept : block(other.block), ptr(other.ptr) {
    acquire_block(block);
}

template <class T>
ShrdPtr<T>::ShrdPtr(ShrdPtr<T>&& other) noexcept : block(other.block), ptr(other.ptr) {
    other.block = nullptr;
    other.ptr = nullptr;
}

template <class T>
template <class U, class>
ShrdPtr<T>::ShrdPtr(const ShrdPtr<U>& other) noexcept : block(other.block), ptr(other.ptr) {
    acquire_block(block);
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
    if (!other) return;

    block = new PointerBlock<U>(other.get());
    ptr = other.release();
}

template <class T>
ShrdPtr<T>& ShrdPtr<T>::operator=(const ShrdPtr<T>& other) noexcept {
    ShrdPtr<T> incoming(other);
    swap(incoming);

    return *this;
}

template <class T>
ShrdPtr<T>& ShrdPtr<T>::operator=(ShrdPtr<T>&& other) noexcept {
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
    // Если конструктор объекта бросит, new сам освободит память блока
    auto* new_block = new InplaceBlock<std::remove_cv_t<T>>(std::forward<Args>(args)...);

    return ShrdPtr<T>(new_block, new_block->object());
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
    return count_owners(block);
}

template <class T>
template <class U>
bool ShrdPtr<T>::operator==(const ShrdPtr<U>& other) const noexcept {
    return ptr == other.ptr;
}

template <class T>
template <class U>
bool ShrdPtr<T>::operator!=(const ShrdPtr<U>& other) const noexcept {
    return !(*this == other);
}

template <class To, class From>
ShrdPtr<To> static_pointer_cast(const ShrdPtr<From>& source) noexcept {
    acquire_block(source.block);

    return ShrdPtr<To>(source.block, static_cast<To*>(source.ptr));
}

template <class T>
void ShrdPtr<T>::reset() noexcept {
    auto* old_block = block;
    block = nullptr;
    ptr = nullptr;
    release_block(old_block);
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
ShrdPtr<T[]>::ShrdPtr(ControlBlock* block, T* data, size_t length) noexcept
    : block(block), data(data), length(length) {}

template <class T>
ShrdPtr<T[]>::ShrdPtr() noexcept : block(nullptr), data(nullptr), length(0) {}

template <class T>
ShrdPtr<T[]>::ShrdPtr(const ShrdPtr<T[]>& other) noexcept
    : block(other.block), data(other.data), length(other.length) {
    acquire_block(block);
}

template <class T>
ShrdPtr<T[]>::ShrdPtr(ShrdPtr<T[]>&& other) noexcept
    : block(other.block), data(other.data), length(other.length) {
    other.block = nullptr;
    other.data = nullptr;
    other.length = 0;
}

template <class T>
ShrdPtr<T[]>::ShrdPtr(UnqPtr<T[]>&& other) : block(nullptr), data(nullptr), length(0) {
    if (!other) return;

    block = new ArrayBlock<T>(other.get());
    length = other.size();
    data = other.release();
}

template <class T>
ShrdPtr<T[]>& ShrdPtr<T[]>::operator=(const ShrdPtr<T[]>& other) noexcept {
    ShrdPtr<T[]> incoming(other);
    swap(incoming);

    return *this;
}

template <class T>
ShrdPtr<T[]>& ShrdPtr<T[]>::operator=(ShrdPtr<T[]>&& other) noexcept {
    ShrdPtr<T[]> incoming(std::move(other));
    swap(incoming);

    return *this;
}

template <class T>
ShrdPtr<T[]> ShrdPtr<T[]>::make_array(size_t count) {

    return ShrdPtr<T[]>(UnqPtr<T[]>::make_array(count));
}

template <class T>
T* ShrdPtr<T[]>::get() const noexcept {
    return data;
}

template <class T>
T& ShrdPtr<T[]>::operator[](size_t index) const {
    if (index >= length) throw std::out_of_range("ShrdPtr array index out of range");

    return data[index];
}

template <class T>
ShrdPtr<T[]>::operator bool() const noexcept {
    return data != nullptr;
}

template <class T>
size_t ShrdPtr<T[]>::size() const noexcept {
    return length;
}

template <class T>
size_t ShrdPtr<T[]>::use_count() const noexcept {
    return count_owners(block);
}

template <class T>
void ShrdPtr<T[]>::reset() noexcept {
    auto* old_block = block;
    block = nullptr;
    data = nullptr;
    length = 0;
    release_block(old_block);
}

template <class T>
void ShrdPtr<T[]>::swap(ShrdPtr<T[]>& other) noexcept {
    std::swap(block, other.block);
    std::swap(data, other.data);
    std::swap(length, other.length);
}

template <class T>
ShrdPtr<T[]>::~ShrdPtr() {
    reset();
}
