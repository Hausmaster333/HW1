#ifndef HW1_LINKED_LIST_H
#define HW1_LINKED_LIST_H

#include "ienumerator.h"
#include <cstddef>
#include <stdexcept>
#include <utility>

template <class T>
class LinkedList {
    private:
        struct Node {
            T data;
            UnqPtr<Node> next;

            Node(const T& item) : data(item) {}
        };

        UnqPtr<Node> head;
        Node* tail;
        size_t length;
        UnqPtr<IteratorState> iterator_state;
    public:
        LinkedList();
        LinkedList(const T* items, size_t count);
        LinkedList(const LinkedList<T>& other);
        LinkedList(LinkedList<T>&& other) noexcept;
        LinkedList<T>& operator=(const LinkedList<T>& other);
        LinkedList<T>& operator=(LinkedList<T>&& other) noexcept;

        const T& get_first() const;
        const T& get_last() const;
        const T& get(size_t index) const;
        size_t get_length() const;

        void append(const T& item);
        void prepend(const T& item);
        void insert_at(const T& item, size_t index);
        void clear() noexcept;
        void swap(LinkedList<T>& other) noexcept;

        class Enumerator : public IEnumerator<T> {
            private:
                const Node* head;
                const Node* current;
                bool started;
                ShrdPtr<IteratorState> state;
                size_t expected_version;

                void check_owner() const {
                    if (!state->alive || state->version != expected_version) {
                        throw std::logic_error("Enumerator owner changed or was destroyed");
                    }
                }
            public:
                Enumerator(const Node* head, ShrdPtr<IteratorState> state)
                    : head(head), current(nullptr), started(false), state(std::move(state)),
                      expected_version(this->state->version) {}

                bool move_next() override {
                    check_owner();
                    if (!started) {
                        current = head;
                        started = true;
                    } else if (current != nullptr) {
                        current = current->next.get();
                    }
                    return current != nullptr;
                }

                const T& get_current() const override {
                    check_owner();
                    if (current == nullptr) throw std::out_of_range("Enumerator has no current item");
                    return current->data;
                }

                void reset() override {
                    check_owner();
                    current = nullptr;
                    started = false;
                }
        };

        UnqPtr<IEnumerator<T>> get_enumerator() const;
        ~LinkedList();
};

#include "linked_list.tpp"

#endif
