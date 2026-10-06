#ifndef UNQ_PTR_H
#define UNQ_PTR_H

#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <class T> class ShrdPtr;

template <class T>
class UnqPtr {
    static_assert(!std::is_array<T>::value, "Use UnqPtr<T[]> for arrays");

    template <class U> friend class UnqPtr;
    template <class U> friend class ShrdPtr;
    private:
        void* object;
        T* ptr;
        void (*deleter)(void*);
        bool can_clone;

        // Принимает объект и способ его удаления
        UnqPtr(void* object, T* ptr, void (*deleter)(void*)) noexcept;
    public:
        // Создаёт пустой указатель
        UnqPtr() noexcept;
        // Запрещает копирование владельца
        UnqPtr(const UnqPtr<T>& other) = delete;
        // Запрещает копирующее присваивание
        UnqPtr<T>& operator=(const UnqPtr<T>& other) = delete;
        // Передаёт владение и опустошает источник
        UnqPtr(UnqPtr<T>&& other) noexcept;
        // Заменяет объект, забирая владение у источника
        UnqPtr<T>& operator=(UnqPtr<T>&& other) noexcept;

        // Передаёт владение с преобразованием типа и запретом clone
        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        UnqPtr(UnqPtr<U>&& other) noexcept;

        // Заменяет объект с преобразованием типа и запретом clone
        template <class U, class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
        UnqPtr<T>& operator=(UnqPtr<U>&& other) noexcept;

        // Создаёт объект с переданными аргументами
        template <class... Args>
        static UnqPtr<T> make(Args&&... args);

        // Копирует объект, если тип копируемый и не был преобразован
        UnqPtr<T> clone() const;

        // Возвращает адрес без передачи владения
        T* get() const noexcept;
        // Возвращает объект, для пустого указателя бросает исключение
        T& operator*() const;
        // Даёт доступ к членам, для пустого указателя бросает исключение
        T* operator->() const;
        // Проверяет наличие объекта
        explicit operator bool() const noexcept;

        // Удаляет объект и очищает указатель
        void reset() noexcept;
        // Обменивает владение двух указателей
        void swap(UnqPtr<T>& other) noexcept;
        // Освобождает принадлежащий объект
        ~UnqPtr();
};

template <class T>
class UnqPtr<T[]> {
    template <class U> friend class ShrdPtr;
    private:
        T* data;
        size_t length;

        // Принимает массив и его длину
        UnqPtr(T* data, size_t length) noexcept;
    public:
        // Создаёт пустой указатель на массив
        UnqPtr() noexcept;
        // Запрещает копирование владельца массива
        UnqPtr(const UnqPtr<T[]>& other) = delete;
        // Запрещает копирующее присваивание
        UnqPtr<T[]>& operator=(const UnqPtr<T[]>& other) = delete;
        // Передаёт массив и опустошает источник
        UnqPtr(UnqPtr<T[]>&& other) noexcept;
        // Заменяет массив, забирая владение у источника
        UnqPtr<T[]>& operator=(UnqPtr<T[]>&& other) noexcept;

        // Создаёт массив указанной длины
        static UnqPtr<T[]> make_array(size_t count);
        // Копирует массив, если элементы допускают копирующее присваивание
        UnqPtr<T[]> clone() const;

        // Возвращает адрес массива без передачи владения
        T* get() const noexcept;
        // Возвращает элемент, при выходе за границы бросает исключение
        T& operator[](size_t index) const;
        // Проверяет наличие массива
        explicit operator bool() const noexcept;
        // Возвращает длину массива
        size_t size() const noexcept;

        // Удаляет массив и очищает указатель
        void reset() noexcept;
        // Обменивает массивы и их длины
        void swap(UnqPtr<T[]>& other) noexcept;
        // Освобождает принадлежащий массив
        ~UnqPtr();
};

#include "unq_ptr.tpp"

#endif
