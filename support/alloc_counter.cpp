#include "alloc_counter.h"
#include <atomic>
#include <cstdlib>
#include <new>

#ifdef ALLOC_COUNTER_SINGLE_THREAD
// Бенчмарк однопоточный: lock-инструкции добавили бы ~5 нс к каждому new и delete
using Counter = size_t;
static void add_to(Counter& counter, size_t value) noexcept { counter += value; }
static size_t read_counter(const Counter& counter) noexcept { return counter; }
#else
// В тестах память выделяют и освобождают и рабочие потоки
using Counter = std::atomic<size_t>;
static void add_to(Counter& counter, size_t value) noexcept {
    counter.fetch_add(value, std::memory_order_relaxed);
}
static size_t read_counter(const Counter& counter) noexcept {
    return counter.load(std::memory_order_relaxed);
}
#endif

static Counter allocations{0};
static Counter deallocations{0};
static Counter bytes{0};
static std::atomic<size_t> fail_countdown{0};

static void* allocate(size_t size) {
    if (fail_countdown.load(std::memory_order_relaxed) != 0 &&
        fail_countdown.fetch_sub(1, std::memory_order_relaxed) == 1) {
        throw std::bad_alloc();
    }

    void* memory = std::malloc(size == 0 ? 1 : size);
    if (memory == nullptr) throw std::bad_alloc();
    add_to(allocations, 1);
    add_to(bytes, size);
    return memory;
}

static void deallocate(void* memory) noexcept {
    if (memory == nullptr) return;
    add_to(deallocations, 1);
    std::free(memory);
}

AllocSnapshot alloc_snapshot() noexcept {
    return {read_counter(allocations), read_counter(deallocations), read_counter(bytes)};
}

void fail_allocation_after(size_t count) noexcept {
    fail_countdown.store(count, std::memory_order_relaxed);
}

void* operator new(size_t size) { return allocate(size); }
void* operator new[](size_t size) { return allocate(size); }

void* operator new(size_t size, const std::nothrow_t&) noexcept {
    try {
        return allocate(size);
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

void* operator new[](size_t size, const std::nothrow_t& tag) noexcept {
    return operator new(size, tag);
}

void operator delete(void* memory) noexcept { deallocate(memory); }
void operator delete[](void* memory) noexcept { deallocate(memory); }
void operator delete(void* memory, size_t) noexcept { deallocate(memory); }
void operator delete[](void* memory, size_t) noexcept { deallocate(memory); }
void operator delete(void* memory, const std::nothrow_t&) noexcept { deallocate(memory); }
void operator delete[](void* memory, const std::nothrow_t&) noexcept { deallocate(memory); }
