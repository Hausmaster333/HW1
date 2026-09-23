#include "linked_list.h"
#include <limits>
#include <stdexcept>

template <class T>
LinkedList<T>::LinkedList()
    : tail(nullptr), length(0), iterator_state(UnqPtr<IteratorState>::make()) {}

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
        const Node* current = other.head.get();
        while (current != nullptr) {
            append(current->data);
            current = current->next.get();
        }
    } catch (...) {
        clear();
        throw;
    }
}

template <class T>
LinkedList<T>::LinkedList(LinkedList<T>&& other) noexcept : LinkedList() {
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
    if (head.get() == nullptr) throw std::out_of_range("List is empty");
    return head->data;
}

template <class T>
const T& LinkedList<T>::get_last() const {
    if (tail == nullptr) throw std::out_of_range("List is empty");
    return tail->data;
}

template <class T>
const T& LinkedList<T>::get(size_t index) const {
    if (index >= length) throw std::out_of_range("Index out of range");

    const Node* current = head.get();
    for (size_t position = 0; position < index; position++) current = current->next.get();
    return current->data;
}

template <class T>
size_t LinkedList<T>::get_length() const {
    return length;
}

template <class T>
void LinkedList<T>::append(const T& item) {
    auto new_node = UnqPtr<Node>::make(item);
    Node* new_tail = new_node.get();

    if (head.get() == nullptr) head = std::move(new_node);
    else tail->next = std::move(new_node);

    tail = new_tail;
    length++;
    iterator_state->version++;
}

template <class T>
void LinkedList<T>::prepend(const T& item) {
    auto new_node = UnqPtr<Node>::make(item);
    Node* new_tail = new_node.get();
    new_node->next = std::move(head);
    head = std::move(new_node);
    if (tail == nullptr) tail = new_tail;
    length++;
    iterator_state->version++;
}

template <class T>
void LinkedList<T>::insert_at(const T& item, size_t index) {
    if (index > length) throw std::out_of_range("Index out of range");
    if (index == 0) { prepend(item); return; }
    if (index == length) { append(item); return; }

    Node* previous = head.get();
    for (size_t position = 1; position < index; position++) previous = previous->next.get();

    auto new_node = UnqPtr<Node>::make(item);
    new_node->next = std::move(previous->next);
    previous->next = std::move(new_node);
    length++;
    iterator_state->version++;
}

template <class T>
void LinkedList<T>::clear() noexcept {
    iterator_state->version++;
    // Перед уничтожением узла забираем next: длинный список не удаляется рекурсивно.
    while (head) {
        auto current = std::move(head);
        head = std::move(current->next);
    }
    tail = nullptr;
    length = 0;
}

template <class T>
void LinkedList<T>::swap(LinkedList<T>& other) noexcept {
    iterator_state->version++;
    other.iterator_state->version++;
    head.swap(other.head);
    std::swap(tail, other.tail);
    std::swap(length, other.length);
}

template <class T>
UnqPtr<IEnumerator<T>> LinkedList<T>::get_enumerator() const {
    return UnqPtr<Enumerator>::make(head.get(), iterator_state.share());
}

template <class T>
LinkedList<T>::~LinkedList() {
    iterator_state->alive = false;
    clear();
}
