#ifndef HW1_IENUMERATOR_H
#define HW1_IENUMERATOR_H

#include "shrd_ptr.h"
#include <cstddef>
#include <stdexcept>
#include <utility>

struct IteratorState {
    bool alive = true;
    size_t version = 0;
};

template <class T>
class IEnumerator {
    public:
        virtual bool move_next() = 0;
        virtual const T& get_current() const = 0;
        virtual void reset() = 0;
        virtual ~IEnumerator() = default;
};

template <class T>
class EnumeratorWrapper {
    private:
        UnqPtr<IEnumerator<T>> iter;
        bool has_current;
    public:
        EnumeratorWrapper(UnqPtr<IEnumerator<T>> iter)
            : iter(std::move(iter)), has_current(false) {}

        bool move_next() {
            has_current = iter->move_next();
            return has_current;
        }

        const T& get_current() const {
            if (!has_current) throw std::logic_error("Enumerator has no current item");
            return iter->get_current();
        }

        void reset() {
            iter->reset();
            has_current = false;
        }
};

#endif
