#ifndef ALLOC_COUNTER_H
#define ALLOC_COUNTER_H

#include <cstddef>

// Счётчик выделений кучи. alloc_counter.cpp подменяет глобальные operator new / delete,
// поэтому считаются все выделения программы, включая сделанные внутри STL.
struct AllocSnapshot {
    size_t allocations;
    size_t deallocations;
    size_t bytes;

    // Число выделений, ещё не освобождённых к моменту снимка
    size_t live() const noexcept { return allocations - deallocations; }
};

// Возвращает текущие значения счётчиков
AllocSnapshot alloc_snapshot() noexcept;

// Заставляет выделение номер count (считая с 1) бросить std::bad_alloc; 0 — выключить
void fail_allocation_after(size_t count) noexcept;

// Выключает отказ выделения при выходе из области видимости
class AllocFailureScope {
    public:
        explicit AllocFailureScope(size_t count) noexcept { fail_allocation_after(count); }
        AllocFailureScope(const AllocFailureScope&) = delete;
        AllocFailureScope& operator=(const AllocFailureScope&) = delete;
        ~AllocFailureScope() { fail_allocation_after(0); }
};

#endif
