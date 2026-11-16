#include "linked_list.h"
#include <limits>
#include <stdexcept>

template <class T>
LinkedList<T>::LinkedList() : length(0), iterator_state(ShrdPtr<IteratorState>::make()) {}

template <class T>
LinkedList<T>::LinkedList(const T* items, size_t count) : LinkedList() {
    if (count > static_cast<size_t>(std::numeric_limits<std::ptrdiff_t>::max())) {
        throw std::length_error("Length is too large");
    }
    if (count > 0 && items == nullptr) throw std::invalid_argument("Items cannot be nullptr");

    try {
        for (size_t index = 0; index < count; index++) append(items[index]);
    } catch (...) {
        clear();
        throw;
    }
}

template <class T>
LinkedList<T>::LinkedList(const LinkedList<T>& other) : LinkedList() {
    try {
        for (NodePtr current = other.head; current; current = current->next) append(current->data);
    } catch (...) {
        clear();
        throw;
    }
}

template <class T>
LinkedList<T>::LinkedList(LinkedList<T>&& other) : LinkedList() {
    swap(other);
}

template <class T>
LinkedList<T>& LinkedList<T>::operator=(const LinkedList<T>& other) {
    if (this == &other) return *this;

    LinkedList<T> copy(other);
    swap(copy);
    return *this;
}

template <class T>
LinkedList<T>& LinkedList<T>::operator=(LinkedList<T>&& other) noexcept {
    if (this == &other) return *this;

    clear();
    swap(other);
    return *this;
}

template <class T>
const T& LinkedList<T>::get_first() const {
    if (!head) throw std::out_of_range("List is empty");

    return head->data;
}

template <class T>
const T& LinkedList<T>::get_last() const {
    NodePtr last = tail.lock();
    if (!last) throw std::out_of_range("List is empty");

    return last->data;
}

template <class T>
const T& LinkedList<T>::get(size_t index) const {
    if (index >= length) throw std::out_of_range("Index out of range");

    NodePtr current = head;
    for (size_t position = 0; position < index; position++) {
        current = current->next;
    }

    return current->data;
}

template <class T>
size_t LinkedList<T>::get_length() const {
    return length;
}

template <class T>
void LinkedList<T>::append(const T& item) {
    NodePtr new_node = NodePtr::make(item);
    NodePtr last = tail.lock();
    tail = new_node;
    if (!last) head = std::move(new_node);
    else last->next = std::move(new_node);

    length++;
    iterator_state->version++;
}

template <class T>
void LinkedList<T>::prepend(const T& item) {
    NodePtr new_node = NodePtr::make(item);
    if (!head) tail = new_node;
    new_node->next = std::move(head);
    head = std::move(new_node);
    length++;
    iterator_state->version++;
}

template <class T>
void LinkedList<T>::insert_at(const T& item, size_t index) {
    if (index > length) throw std::out_of_range("Index out of range");
    if (index == 0) { prepend(item); return; }
    if (index == length) { append(item); return; }

    NodePtr previous = head;
    for (size_t position = 1; position < index; position++) previous = previous->next;

    NodePtr new_node = NodePtr::make(item);
    new_node->next = std::move(previous->next);
    previous->next = std::move(new_node);
    length++;
    iterator_state->version++;
}

template <class T>
void LinkedList<T>::clear() noexcept {
    iterator_state->version++;

    while (head) {
        auto current = std::move(head);
        head = std::move(current->next);
    }
    tail.reset();
    length = 0;
}

template <class T>
void LinkedList<T>::swap(LinkedList<T>& other) noexcept {
    iterator_state->version++;
    other.iterator_state->version++;
    head.swap(other.head);
    tail.swap(other.tail);
    std::swap(length, other.length);
}

template <class T>
UnqPtr<IEnumerator<T>> LinkedList<T>::get_enumerator() const {
    return UnqPtr<Enumerator>::make(head, iterator_state);
}

template <class T>
LinkedList<T>::~LinkedList() {
    iterator_state->alive = false;
    clear();
}
