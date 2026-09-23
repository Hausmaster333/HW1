#include "sequence.h"
#include <limits>
#include <stdexcept>

template <class T>
UnqPtr<Sequence<T>> Sequence<T>::get_sub_sequence(size_t start, size_t end) const {
    if (end < start || end >= get_count()) {
        throw std::out_of_range("Index out of range");
    }

    auto result = CreateEmpty();
    EnumeratorWrapper<T> iter(get_enumerator());
    size_t index = 0;
    while (index <= end && iter.move_next()) {
        if (index >= start) result->sys_append(iter.get_current());
        index++;
    }
    return result;
}

template <class T>
UnqPtr<Sequence<T>> Sequence<T>::concat(const Sequence<T>* other) const {
    if (other == nullptr) throw std::invalid_argument("Cannot concat with nullptr");

    auto result = CreateEmpty();
    EnumeratorWrapper<T> first(get_enumerator());
    while (first.move_next()) result->sys_append(first.get_current());

    EnumeratorWrapper<T> second(other->get_enumerator());
    while (second.move_next()) result->sys_append(second.get_current());
    return result;
}

template <class T>
UnqPtr<Sequence<T>> Sequence<T>::map(T (*func)(const T& elem)) const {
    if (func == nullptr) throw std::invalid_argument("Cannot map with nullptr function");

    auto result = CreateEmpty();
    EnumeratorWrapper<T> iter(get_enumerator());
    while (iter.move_next()) result->sys_append(func(iter.get_current()));
    return result;
}

template <class T>
UnqPtr<Sequence<T>> Sequence<T>::where(bool (*predicate)(const T& elem)) const {
    if (predicate == nullptr) throw std::invalid_argument("Cannot where with nullptr predicate");

    auto result = CreateEmpty();
    EnumeratorWrapper<T> iter(get_enumerator());
    while (iter.move_next()) {
        const T& item = iter.get_current();
        if (predicate(item)) result->sys_append(item);
    }
    return result;
}

template <class T>
T Sequence<T>::reduce(T (*func)(const T& accumulator, const T& current),
                      const T& initial_elem) const {
    if (func == nullptr) throw std::invalid_argument("Cannot reduce with nullptr function");

    T result = initial_elem;
    EnumeratorWrapper<T> iter(get_enumerator());
    while (iter.move_next()) result = func(result, iter.get_current());
    return result;
}

template <class T>
UnqPtr<Sequence<T>> Sequence<T>::slice(std::ptrdiff_t index, std::ptrdiff_t count,
                                        const Sequence<T>* replace_seq) const {
    if (count < 0) throw std::invalid_argument("Slice count cannot be negative");

    size_t length = get_count();
    if (length > static_cast<size_t>(std::numeric_limits<std::ptrdiff_t>::max())) {
        throw std::length_error("Sequence is too long for signed slice index");
    }
    if (index < 0) index += static_cast<std::ptrdiff_t>(length);
    if (index < 0 || static_cast<size_t>(index) >= length) {
        throw std::out_of_range("Slice index out of range");
    }
    size_t first = static_cast<size_t>(index);
    size_t removed = static_cast<size_t>(count);
    if (removed > length - first) removed = length - first;

    auto result = CreateEmpty();
    EnumeratorWrapper<T> iter(get_enumerator());
    size_t position = 0;
    while (iter.move_next()) {
        if (position < first) result->sys_append(iter.get_current());
        if (position == first && replace_seq != nullptr) {
            EnumeratorWrapper<T> replacement(replace_seq->get_enumerator());
            while (replacement.move_next()) result->sys_append(replacement.get_current());
        }
        if (position >= first + removed) result->sys_append(iter.get_current());
        position++;
    }
    return result;
}

template <class T>
ArraySequence<T>::ArraySequence() : count(0) {}

template <class T>
ArraySequence<T>::ArraySequence(const T* items, size_t count) : array(items, count), count(count) {}

template <class T>
ArraySequence<T>::ArraySequence(const DynamicArray<T>& other)
    : array(other), count(other.get_size()) {}

template <class T>
ArraySequence<T>::ArraySequence(const ArraySequence<T>& other)
    : array(other.array), count(other.count) {}

template <class T>
void ArraySequence<T>::sys_append(const T& item) {
    T saved = item; // item может ссылаться на элемент array, который удалит resize.
    if (count == array.get_size()) {
        size_t new_size = (count == 0) ? 4 : count * 2;
        array.resize(new_size);
    }
    array.set(count, saved);
    count++;
}

template <class T>
void ArraySequence<T>::sys_prepend(const T& item) {
    T saved = item;
    if (count == array.get_size()) {
        size_t new_size = (count == 0) ? 4 : count * 2;
        array.resize(new_size);
    }
    for (size_t index = count; index > 0; index--) array.set(index, array.get(index - 1));
    array.set(0, saved);
    count++;
}

template <class T>
void ArraySequence<T>::sys_insert_at(const T& item, size_t index) {
    if (index > count) throw std::out_of_range("Index out of range");
    T saved = item;
    if (count == array.get_size()) {
        size_t new_size = (count == 0) ? 4 : count * 2;
        array.resize(new_size);
    }
    for (size_t position = count; position > index; position--) {
        array.set(position, array.get(position - 1));
    }
    array.set(index, saved);
    count++;
}

template <class T>
const T& ArraySequence<T>::get_first() const {
    if (count == 0) throw std::out_of_range("Sequence is empty");
    return array.get(0);
}

template <class T>
const T& ArraySequence<T>::get_last() const {
    if (count == 0) throw std::out_of_range("Sequence is empty");
    return array.get(count - 1);
}

template <class T>
const T& ArraySequence<T>::get(size_t index) const {
    if (index >= count) throw std::out_of_range("Index out of range");
    return array.get(index);
}

template <class T>
std::optional<T> ArraySequence<T>::try_get_first() const {
    if (count == 0) return std::nullopt;
    return array.get(0);
}

template <class T>
std::optional<T> ArraySequence<T>::try_get_last() const {
    if (count == 0) return std::nullopt;
    return array.get(count - 1);
}

template <class T>
std::optional<T> ArraySequence<T>::try_get(size_t index) const {
    if (index >= count) return std::nullopt;
    return array.get(index);
}

template <class T>
size_t ArraySequence<T>::get_count() const {
    return count;
}

template <class T>
UnqPtr<IEnumerator<T>> ArraySequence<T>::get_enumerator() const {
    return array.get_enumerator(count);
}

template <class T>
ListSequence<T>::ListSequence() = default;

template <class T>
ListSequence<T>::ListSequence(const T* items, size_t count) : list(items, count) {}

template <class T>
ListSequence<T>::ListSequence(const LinkedList<T>& other) : list(other) {}

template <class T>
ListSequence<T>::ListSequence(const ListSequence<T>& other) : list(other.list) {}

template <class T>
void ListSequence<T>::sys_append(const T& item) {
    list.append(item);
}

template <class T>
void ListSequence<T>::sys_prepend(const T& item) {
    list.prepend(item);
}

template <class T>
void ListSequence<T>::sys_insert_at(const T& item, size_t index) {
    list.insert_at(item, index);
}

template <class T>
const T& ListSequence<T>::get_first() const {
    return list.get_first();
}

template <class T>
const T& ListSequence<T>::get_last() const {
    return list.get_last();
}

template <class T>
const T& ListSequence<T>::get(size_t index) const {
    return list.get(index);
}

template <class T>
std::optional<T> ListSequence<T>::try_get_first() const {
    if (list.get_length() == 0) return std::nullopt;
    return list.get_first();
}

template <class T>
std::optional<T> ListSequence<T>::try_get_last() const {
    if (list.get_length() == 0) return std::nullopt;
    return list.get_last();
}

template <class T>
std::optional<T> ListSequence<T>::try_get(size_t index) const {
    if (index >= list.get_length()) return std::nullopt;
    return list.get(index);
}

template <class T>
size_t ListSequence<T>::get_count() const {
    return list.get_length();
}

template <class T>
UnqPtr<IEnumerator<T>> ListSequence<T>::get_enumerator() const {
    return list.get_enumerator();
}
