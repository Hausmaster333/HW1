#ifndef HW1_SEQUENCE_H
#define HW1_SEQUENCE_H

#include "dynamic_array.h"
#include "linked_list.h"
#include <cstddef>
#include <optional>

template <class T>
class Sequence {
    protected:
        virtual void sys_append(const T& item) = 0;
        virtual UnqPtr<Sequence<T>> CreateEmpty() const = 0;
    public:
        virtual const T& get_first() const = 0;
        virtual const T& get_last() const = 0;
        virtual std::optional<T> try_get_first() const = 0;
        virtual std::optional<T> try_get_last() const = 0;
        virtual size_t get_count() const = 0;

        UnqPtr<Sequence<T>> get_sub_sequence(size_t start, size_t end) const;
        UnqPtr<Sequence<T>> concat(const Sequence<T>* other) const;
        UnqPtr<Sequence<T>> map(T (*func)(const T& elem)) const;
        UnqPtr<Sequence<T>> where(bool (*predicate)(const T& elem)) const;
        T reduce(T (*func)(const T& accumulator, const T& current), const T& initial_elem) const;
        UnqPtr<Sequence<T>> slice(std::ptrdiff_t index, std::ptrdiff_t count,
                                   const Sequence<T>* replace_seq = nullptr) const;

        virtual UnqPtr<IEnumerator<T>> get_enumerator() const = 0;
        virtual ~Sequence() = default;
};

template <class T>
class ArraySequence : public Sequence<T> {
    protected:
        DynamicArray<T> array;
        size_t count;

        void sys_append(const T& item) override;
        void sys_prepend(const T& item);
        void sys_insert_at(const T& item, size_t index);
    public:
        ArraySequence();
        ArraySequence(const T* items, size_t count);
        ArraySequence(const DynamicArray<T>& other);
        ArraySequence(const ArraySequence<T>& other);

        const T& get_first() const override;
        const T& get_last() const override;
        const T& get(size_t index) const;
        std::optional<T> try_get_first() const override;
        std::optional<T> try_get_last() const override;
        std::optional<T> try_get(size_t index) const;
        size_t get_count() const override;
        UnqPtr<IEnumerator<T>> get_enumerator() const override;
};

template <class T>
class ListSequence : public Sequence<T> {
    protected:
        LinkedList<T> list;

        void sys_append(const T& item) override;
        void sys_prepend(const T& item);
        void sys_insert_at(const T& item, size_t index);
    public:
        ListSequence();
        ListSequence(const T* items, size_t count);
        ListSequence(const LinkedList<T>& other);
        ListSequence(const ListSequence<T>& other);

        const T& get_first() const override;
        const T& get_last() const override;
        const T& get(size_t index) const;
        std::optional<T> try_get_first() const override;
        std::optional<T> try_get_last() const override;
        std::optional<T> try_get(size_t index) const;
        size_t get_count() const override;
        UnqPtr<IEnumerator<T>> get_enumerator() const override;
};

template <class T, class Derived, bool Mutable, template <class> class Base>
class SequenceCRTP : public Base<T> {
    protected:
        UnqPtr<Sequence<T>> CreateEmpty() const override {
            return UnqPtr<Derived>::make();
        }
    public:
        using Base<T>::Base;

        // Для Mutable возвращаем ссылку, для Immutable — владеющую копию.
        decltype(auto) append(const T& item) {
            if constexpr (Mutable) {
                this->sys_append(item);
                return static_cast<Derived&>(*this);
            } else {
                auto result = Clone();
                result->sys_append(item);
                return result;
            }
        }

        decltype(auto) prepend(const T& item) {
            if constexpr (Mutable) {
                this->sys_prepend(item);
                return static_cast<Derived&>(*this);
            } else {
                auto result = Clone();
                result->sys_prepend(item);
                return result;
            }
        }

        decltype(auto) insert_at(const T& item, size_t index) {
            if constexpr (Mutable) {
                this->sys_insert_at(item, index);
                return static_cast<Derived&>(*this);
            } else {
                auto result = Clone();
                result->sys_insert_at(item, index);
                return result;
            }
        }

        UnqPtr<Derived> Clone() const {
            return UnqPtr<Derived>::make(static_cast<const Derived&>(*this));
        }

        UnqPtr<Derived> Empty() const {
            return UnqPtr<Derived>::make();
        }

        Derived& chain_append(const T& item) {
            static_assert(Mutable, "chain_append is only available for Mutable sequences");
            return append(item);
        }

        Derived& chain_prepend(const T& item) {
            static_assert(Mutable, "chain_prepend is only available for Mutable sequences");
            return prepend(item);
        }

        Derived& chain_insert_at(const T& item, size_t index) {
            static_assert(Mutable, "chain_insert_at is only available for Mutable sequences");
            return insert_at(item, index);
        }
};

template <class T>
class MutableArraySequence : public SequenceCRTP<T, MutableArraySequence<T>, true, ArraySequence> {
    public:
        using SequenceCRTP<T, MutableArraySequence<T>, true, ArraySequence>::SequenceCRTP;
};

template <class T>
class ImmutableArraySequence : public SequenceCRTP<T, ImmutableArraySequence<T>, false, ArraySequence> {
    public:
        using SequenceCRTP<T, ImmutableArraySequence<T>, false, ArraySequence>::SequenceCRTP;
};

template <class T>
class MutableListSequence : public SequenceCRTP<T, MutableListSequence<T>, true, ListSequence> {
    public:
        using SequenceCRTP<T, MutableListSequence<T>, true, ListSequence>::SequenceCRTP;
};

template <class T>
class ImmutableListSequence : public SequenceCRTP<T, ImmutableListSequence<T>, false, ListSequence> {
    public:
        using SequenceCRTP<T, ImmutableListSequence<T>, false, ListSequence>::SequenceCRTP;
};

#include "sequence.tpp"

#endif
