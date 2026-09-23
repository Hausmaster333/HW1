#ifndef HW1_DYNAMIC_ARRAY_H
#define HW1_DYNAMIC_ARRAY_H

#include "ienumerator.h"
#include <cstddef>
#include <stdexcept>

template <class T>
class DynamicArray {
    private:
        UnqPtr<T[]> data;
        size_t size;
        UnqPtr<IteratorState> iterator_state;
    public:
        DynamicArray();
        DynamicArray(size_t size);
        DynamicArray(const T* items, size_t count);
        DynamicArray(const DynamicArray<T>& other);
        DynamicArray<T>& operator=(const DynamicArray<T>& other);

        const T& get(size_t index) const;
        size_t get_size() const;
        void set(size_t index, const T& value);
        void resize(size_t new_size);
        void swap(DynamicArray<T>& other) noexcept;
        ~DynamicArray();

        class Enumerator : public IEnumerator<T> {
            private:
                const T* data;
                size_t size;
                size_t index;
                bool has_current;
                ShrdPtr<IteratorState> state;
                size_t expected_version;

                void check_owner() const {
                    if (!state->alive || state->version != expected_version) {
                        throw std::logic_error("Enumerator owner changed or was destroyed");
                    }
                }
            public:
                Enumerator(const T* data, size_t size, ShrdPtr<IteratorState> state)
                    : data(data), size(size), index(0), has_current(false), state(std::move(state)),
                      expected_version(this->state->version) {}

                bool move_next() override {
                    check_owner();
                    if (index >= size) {
                        has_current = false;
                        return false;
                    }
                    index++;
                    has_current = true;
                    return true;
                }

                const T& get_current() const override {
                    check_owner();
                    if (!has_current) throw std::out_of_range("Enumerator is out of range");
                    return data[index - 1];
                }

                void reset() override {
                    check_owner();
                    index = 0;
                    has_current = false;
                }
        };

        UnqPtr<IEnumerator<T>> get_enumerator() const;
        UnqPtr<IEnumerator<T>> get_enumerator(size_t count) const;
};

#include "dynamic_array.tpp"

#endif
