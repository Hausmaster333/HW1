#include "dynamic_array.h"
#include <limits>
#include <stdexcept>
#include <utility>

template <class T>
DynamicArray<T>::DynamicArray() : size(0), iterator_state(UnqPtr<IteratorState>::make()) {}

template <class T>
DynamicArray<T>::DynamicArray(size_t size)
    : size(size), iterator_state(UnqPtr<IteratorState>::make()) {
    if (size > static_cast<size_t>(std::numeric_limits<std::ptrdiff_t>::max())) {
        throw std::length_error("Size is too large");
    }
    data = UnqPtr<T[]>::make_array(size);
}

template <class T>
DynamicArray<T>::DynamicArray(const T* items, size_t count) : DynamicArray(count) {
    if (count > 0 && items == nullptr) throw std::invalid_argument("Items cannot be nullptr");
    for (size_t index = 0; index < count; index++) data[index] = items[index];
}

template <class T>
DynamicArray<T>::DynamicArray(const DynamicArray<T>& other) : DynamicArray(other.size) {
    for (size_t index = 0; index < size; index++) data[index] = other.data[index];
}

template <class T>
DynamicArray<T>& DynamicArray<T>::operator=(const DynamicArray<T>& other) {
    if (this == &other) return *this;

    DynamicArray<T> copy(other);
    swap(copy);
    return *this;
}

template <class T>
const T& DynamicArray<T>::get(size_t index) const {
    if (index >= size) throw std::out_of_range("Index out of range");
    return data[index];
}

template <class T>
size_t DynamicArray<T>::get_size() const {
    return size;
}

template <class T>
void DynamicArray<T>::set(size_t index, const T& value) {
    if (index >= size) throw std::out_of_range("Index out of range");
    iterator_state->version++;
    data[index] = value;
}

template <class T>
void DynamicArray<T>::resize(size_t new_size) {
    if (new_size > static_cast<size_t>(std::numeric_limits<std::ptrdiff_t>::max())) {
        throw std::length_error("Size is too large");
    }

    DynamicArray<T> resized(new_size);
    size_t copy_count = (new_size < size) ? new_size : size;
    for (size_t index = 0; index < copy_count; index++) resized.data[index] = data[index];
    swap(resized);
}

template <class T>
void DynamicArray<T>::swap(DynamicArray<T>& other) noexcept {
    iterator_state->version++;
    other.iterator_state->version++;
    data.swap(other.data);
    std::swap(size, other.size);
}

template <class T>
DynamicArray<T>::~DynamicArray() {
    iterator_state->alive = false;
}

template <class T>
UnqPtr<IEnumerator<T>> DynamicArray<T>::get_enumerator() const {
    return get_enumerator(size);
}

template <class T>
UnqPtr<IEnumerator<T>> DynamicArray<T>::get_enumerator(size_t count) const {
    if (count > size) throw std::out_of_range("Enumerator count out of range");
    return UnqPtr<Enumerator>::make(data.get(), count, iterator_state.share());
}
