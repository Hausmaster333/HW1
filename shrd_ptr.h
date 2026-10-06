#ifndef SHRD_PTR_H
#define SHRD_PTR_H

#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include "unq_ptr.h"

namespace decentral_detail {
    struct ControlBlock {
        void* object = nullptr;
        void (*deleter)(void*);
        size_t references = 1;
        size_t length;

        // Сохраняет способ удаления и длину массива для первого владельца
        ControlBlock(void (*deleter)(void*), size_t length = 0) noexcept
            : deleter(deleter), length(length) {}
        // Запрещает копирование блока владения
        ControlBlock(const ControlBlock&) = delete;
        // Запрещает копирующее присваивание блока
        ControlBlock& operator=(const ControlBlock&) = delete;
        // Удаляет объект сохранённым способом
        ~ControlBlock() { deleter(object); }
    };
}

template <class T>
class ShrdPtr {
    static_assert(!std::is_array<T>::value, "Use ShrdPtr<T[]> for arrays");

    template <class U> friend class ShrdPtr;
    private:
        decentral_detail::ControlBlock* block;
        T* ptr;

        // Принимает готовый блок без увеличения счётчика владельцев
        ShrdPtr(decentral_detail::ControlBlock* block, T* ptr) noexcept;
    public:
        // Создаёт пустой указатель
        ShrdPtr() noexcept;
        // Добавляет владельца того же объекта
        ShrdPtr(const ShrdPtr<T>& other) noexcept;
        // Передаёт долю владения и опустошает источник
        ShrdPtr(ShrdPtr<T>&& other) noexcept;

        // Добавляет владельца с преобразованием типа указателя
        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        ShrdPtr(const ShrdPtr<U>& other) noexcept;

        // Передаёт долю владения с преобразованием типа указателя
        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        ShrdPtr(ShrdPtr<U>&& other) noexcept;

        // Забирает объект у UnqPtr и создаёт общее владение
        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        ShrdPtr(UnqPtr<U>&& other);

        // Отпускает прежний объект и разделяет владение с источником
        ShrdPtr<T>& operator=(const ShrdPtr<T>& other) noexcept;
        // Отпускает прежний объект и забирает долю владения источника
        ShrdPtr<T>& operator=(ShrdPtr<T>&& other) noexcept;

        // Заменяет долю владения копией с преобразованием типа
        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        ShrdPtr<T>& operator=(const ShrdPtr<U>& other) noexcept;

        // Заменяет долю владения перемещением с преобразованием типа
        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        ShrdPtr<T>& operator=(ShrdPtr<U>&& other) noexcept;

        // Создаёт объект с переданными аргументами и блок владения
        template <class... Args>
        static ShrdPtr<T> make(Args&&... args);

        // Возвращает адрес без передачи владения
        T* get() const noexcept;
        // Возвращает объект, для пустого указателя бросает исключение
        T& operator*() const;
        // Даёт доступ к членам, для пустого указателя бросает исключение
        T* operator->() const;
        // Проверяет наличие объекта
        explicit operator bool() const noexcept;
        // Возвращает число владельцев объекта
        size_t use_count() const noexcept;

        // Очищает указатель, последний владелец удаляет объект
        void reset() noexcept;
        // Обменивает доли владения двух указателей
        void swap(ShrdPtr<T>& other) noexcept;
        // Отпускает долю владения, последний владелец удаляет объект
        ~ShrdPtr();
};

template <class T>
class ShrdPtr<T[]> {
    private:
        decentral_detail::ControlBlock* block;

        // Принимает готовый блок без увеличения счётчика владельцев
        explicit ShrdPtr(decentral_detail::ControlBlock* block) noexcept;
    public:
        // Создаёт пустой указатель на массив
        ShrdPtr() noexcept;
        // Добавляет владельца того же массива
        ShrdPtr(const ShrdPtr<T[]>& other) noexcept;
        // Передаёт долю владения массивом и опустошает источник
        ShrdPtr(ShrdPtr<T[]>&& other) noexcept;
        // Забирает массив у UnqPtr и создаёт общее владение
        ShrdPtr(UnqPtr<T[]>&& other);
        // Отпускает прежний массив и разделяет владение с источником
        ShrdPtr<T[]>& operator=(const ShrdPtr<T[]>& other) noexcept;
        // Отпускает прежний массив и забирает долю владения источника
        ShrdPtr<T[]>& operator=(ShrdPtr<T[]>&& other) noexcept;

        // Создаёт массив указанной длины и блок владения
        static ShrdPtr<T[]> make_array(size_t count);

        // Возвращает адрес массива без передачи владения
        T* get() const noexcept;
        // Возвращает элемент, при выходе за границы бросает исключение
        T& operator[](size_t index) const;
        // Проверяет наличие массива
        explicit operator bool() const noexcept;
        // Возвращает длину массива
        size_t size() const noexcept;
        // Возвращает число владельцев массива
        size_t use_count() const noexcept;

        // Очищает указатель, последний владелец удаляет массив
        void reset() noexcept;
        // Обменивает доли владения массивами
        void swap(ShrdPtr<T[]>& other) noexcept;
        // Отпускает долю владения, последний владелец удаляет массив
        ~ShrdPtr();
};

#include "shrd_ptr.tpp"

#endif
