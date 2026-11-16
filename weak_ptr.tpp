#include "weak_ptr.h"
#include <atomic>
#include <cstdint>
#include <utility>

inline void acquire_weak(ControlBlock* block) noexcept {
    if (block != nullptr) block->weak_references.fetch_add(1, std::memory_order_relaxed);
}

template <class T>
WeakPtr<T>::WeakPtr() noexcept : block(nullptr), ptr(nullptr) {}

template <class T>
WeakPtr<T>::WeakPtr(const WeakPtr<T>& other) noexcept : block(other.block), ptr(other.ptr) {
    acquire_weak(block);
}

template <class T>
WeakPtr<T>::WeakPtr(WeakPtr<T>&& other) noexcept : block(other.block), ptr(other.ptr) {
    other.block = nullptr;
    other.ptr = nullptr;
}

template <class T>
template <class U, class>
WeakPtr<T>::WeakPtr(const ShrdPtr<U>& owner) noexcept : block(owner.block), ptr(owner.ptr) {
    acquire_weak(block);
}

template <class T>
WeakPtr<T>& WeakPtr<T>::operator=(const WeakPtr<T>& other) noexcept {
    WeakPtr<T> incoming(other);
    swap(incoming);

    return *this;
}

template <class T>
WeakPtr<T>& WeakPtr<T>::operator=(WeakPtr<T>&& other) noexcept {
    WeakPtr<T> incoming(std::move(other));
    swap(incoming);

    return *this;
}

template <class T>
template <class U, class>
WeakPtr<T>& WeakPtr<T>::operator=(const ShrdPtr<U>& owner) noexcept {
    WeakPtr<T> incoming(owner);
    swap(incoming);

    return *this;
}

template <class T>
ShrdPtr<T> WeakPtr<T>::lock() const noexcept {
    if (block == nullptr) return ShrdPtr<T>();

    std::uint32_t owners = block->references.load(std::memory_order_relaxed);
    do {
        if (owners == 0) return ShrdPtr<T>();
    } while (!block->references.compare_exchange_weak(owners, owners + 1, std::memory_order_acq_rel, std::memory_order_relaxed));

    return ShrdPtr<T>(block, ptr);
}

template <class T>
bool WeakPtr<T>::expired() const noexcept {
    return use_count() == 0;
}

template <class T>
size_t WeakPtr<T>::use_count() const noexcept {
    return count_owners(block);
}

template <class T>
void WeakPtr<T>::reset() noexcept {
    auto* old_block = block;
    block = nullptr;
    ptr = nullptr;
    if (old_block != nullptr) release_weak(old_block);
}

template <class T>
void WeakPtr<T>::swap(WeakPtr<T>& other) noexcept {
    std::swap(block, other.block);
    std::swap(ptr, other.ptr);
}

template <class T>
WeakPtr<T>::~WeakPtr() {
    reset();
}
