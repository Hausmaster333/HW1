#ifndef UNQ_PTR_H
#define UNQ_PTR_H

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace decentral_detail {
    struct Record {
        bool has_owner = true;
        size_t descriptors = 0;

        virtual Record* clone() const = 0;
        virtual ~Record() = default;
    };

    template <class T>
    struct ObjectRecord : Record {
        template <class... Args>
        ObjectRecord(Args&&... args) : value(std::forward<Args>(args)...) {}

        Record* clone() const override {
            if constexpr (std::is_copy_constructible<T>::value) {
                return new ObjectRecord<T>(value);
            } else {
                throw std::logic_error("Object type cannot be cloned");
            }
        }
        T value;
    };

    template <class T>
    struct ArrayRecord : Record {
        explicit ArrayRecord(size_t count) : data(new T[count]()), count(count) {}
        ~ArrayRecord() override { delete[] data; }

        ArrayRecord<T>* clone() const override {
            if constexpr (std::is_copy_assignable<T>::value) {
                auto* copy = new ArrayRecord<T>(count);
                try {
                    for (size_t index = 0; index < count; index++) copy->data[index] = data[index];
                } catch (...) {
                    delete copy;
                    throw;
                }
                return copy;
            } else {
                throw std::logic_error("Array element type cannot be cloned");
            }
        }

        T* data;
        size_t count;
    };
}

template <class T> class ShrdPtr;

template <class T>
class UnqPtr {
    static_assert(!std::is_array<T>::value, "Use UnqPtr<T[]> for arrays");

    template <class U> friend class UnqPtr;
    template <class U> friend class ShrdPtr;
    private:
        decentral_detail::Record* record;
        // После clone() на новом Record снова получаем правильный Base*.
        std::function<T*(decentral_detail::Record*)> view;

        UnqPtr(decentral_detail::Record* record,
               std::function<T*(decentral_detail::Record*)>&& view) noexcept;
    public:
        UnqPtr() noexcept;
        UnqPtr(const UnqPtr<T>& other) = delete;
        UnqPtr<T>& operator=(const UnqPtr<T>& other) = delete;
        UnqPtr(UnqPtr<T>&& other) noexcept;
        UnqPtr<T>& operator=(UnqPtr<T>&& other) noexcept;

        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        UnqPtr(UnqPtr<U>&& other);

        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        UnqPtr<T>& operator=(UnqPtr<U>&& other);

        template <class... Args>
        static UnqPtr<T> make(Args&&... args);

        UnqPtr<T> clone() const;
        ShrdPtr<T> share() const noexcept;

        T* get() const noexcept;
        T& operator*() const;
        T* operator->() const;
        explicit operator bool() const noexcept;
        size_t descriptor_count() const noexcept;

        void reset() noexcept;
        void swap(UnqPtr<T>& other) noexcept;
        ~UnqPtr();
};

template <class T>
class UnqPtr<T[]> {
    friend class ShrdPtr<T[]>;
    private:
        decentral_detail::ArrayRecord<T>* record;

        explicit UnqPtr(decentral_detail::ArrayRecord<T>* record) noexcept;
    public:
        UnqPtr() noexcept;
        UnqPtr(const UnqPtr<T[]>& other) = delete;
        UnqPtr<T[]>& operator=(const UnqPtr<T[]>& other) = delete;
        UnqPtr(UnqPtr<T[]>&& other) noexcept;
        UnqPtr<T[]>& operator=(UnqPtr<T[]>&& other) noexcept;

        static UnqPtr<T[]> make_array(size_t count);
        UnqPtr<T[]> clone() const;
        ShrdPtr<T[]> share() const noexcept;

        T* get() const noexcept;
        T& operator[](size_t index) const;
        explicit operator bool() const noexcept;
        size_t size() const noexcept;
        size_t descriptor_count() const noexcept;

        void reset() noexcept;
        void swap(UnqPtr<T[]>& other) noexcept;
        ~UnqPtr();
};

#include "unq_ptr.tpp"
#include "shrd_ptr.h"

#endif
