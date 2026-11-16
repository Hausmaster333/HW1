#include "sequence/sequence.h"
#include "support/alloc_counter.h"
#include <gtest/gtest.h>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

int double_value(const int& value) { return value * 2; }
bool is_even(const int& value) { return value % 2 == 0; }
int add_values(const int& left, const int& right) { return left + right; }

struct Counted {
    static int alive;
    int value;

    Counted(int value = 0) : value(value) { alive++; }
    Counted(const Counted& other) : value(other.value) { alive++; }
    Counted& operator=(const Counted& other) {
        value = other.value;
        return *this;
    }
    ~Counted() { alive--; }
};

int Counted::alive = 0;

struct NoDefault {
    int value;

    NoDefault() = delete;
    NoDefault(int value) : value(value) {}
};

Counted fail_on_two(const Counted& item) {
    if (item.value == 2) throw std::runtime_error("Map failed");
    return item;
}

// Во время компиляции проверяем типы результатов Mutable/Immutable и типы размеров контейнеров.
static_assert(std::is_same<decltype(std::declval<MutableArraySequence<int>&>().append(1)),
                           MutableArraySequence<int>&>::value,
              "Mutable append must return a reference");
static_assert(std::is_same<decltype(std::declval<ImmutableArraySequence<int>&>().append(1)),
                           UnqPtr<ImmutableArraySequence<int>>>::value,
              "Immutable append must return an owning pointer");
static_assert(std::is_same<decltype(std::declval<LinkedList<int>&>().get_length()), size_t>::value,
              "List length must use size_t");
static_assert(std::is_same<decltype(std::declval<DynamicArray<int>&>().get_size()), size_t>::value,
              "Array size must use size_t");

// ---------------------------------------------------------------- DynamicArray

// Копия массива имеет собственный буфер: её изменение не затрагивает оригинал.
TEST(DynamicArrayTest, CopyIsIndependent) {
    int items[] = {1, 2, 3};
    DynamicArray<int> original(items, 3);
    DynamicArray<int> copy(original);
    copy.set(1, 9);
    EXPECT_EQ(original.get(1), 2);
}

// resize() сохраняет старые элементы, новые заполняет нулями, оригинал не трогает.
TEST(DynamicArrayTest, ResizeOfCopyKeepsElements) {
    int items[] = {1, 2, 3};
    DynamicArray<int> original(items, 3);
    DynamicArray<int> copy(original);
    copy.set(1, 9);
    copy.resize(5);
    EXPECT_EQ(copy.get_size(), 5u);
    EXPECT_EQ(copy.get(1), 9);
    EXPECT_EQ(copy.get(4), 0);
    EXPECT_EQ(original.get_size(), 3u);
}

// После конца массива перечислитель остаётся завершённым и не выдаёт последний элемент повторно.
TEST(DynamicArrayTest, EnumeratorDoesNotRestartAfterEnd) {
    int items[] = {5};
    DynamicArray<int> array(items, 1);
    auto iter = array.get_enumerator();

    EXPECT_TRUE(iter->move_next());
    EXPECT_EQ(iter->get_current(), 5);
    EXPECT_FALSE(iter->move_next());
    EXPECT_FALSE(iter->move_next());
    EXPECT_THROW(iter->get_current(), std::out_of_range);
}

// ---------------------------------------------------------------- ArraySequence

TEST(SequenceArrayTest, TryGetOnEmptyIsEmpty) {
    MutableArraySequence<int> sequence;
    EXPECT_FALSE(sequence.try_get_first().has_value());
    EXPECT_FALSE(sequence.try_get_last().has_value());
    EXPECT_FALSE(sequence.try_get(0).has_value());
}

TEST(SequenceArrayTest, TryGetReturnsEmptyOutsideBounds) {
    MutableArraySequence<int> sequence;
    sequence.append(5);
    EXPECT_EQ(sequence.try_get_first().value(), 5);
    EXPECT_EQ(sequence.try_get_last().value(), 5);
    EXPECT_EQ(sequence.try_get(0).value(), 5);
    EXPECT_FALSE(sequence.try_get(1).has_value());
}

// Mutable-вставки меняют исходный массив и возвращают его же.
TEST(SequenceArrayTest, MutableOperationsKeepSameObject) {
    MutableArraySequence<int> sequence;
    EXPECT_EQ(&sequence.append(2), &sequence);
    sequence.prepend(1);
    sequence.insert_at(3, 2); // Значение 3 на позицию 2
    EXPECT_EQ(sequence.get_count(), 3u);
    EXPECT_EQ(sequence.get_first(), 1);
    EXPECT_EQ(sequence.get_last(), 3);
}

TEST(SequenceArrayTest, ChainedCalls) {
    int items[] = {1, 2, 3};
    MutableArraySequence<int> sequence(items, 3);
    sequence.chain_append(4).chain_prepend(0);
    EXPECT_EQ(sequence.get_count(), 5u);
    EXPECT_EQ(sequence.get(0), 0);
    EXPECT_EQ(sequence.get(4), 4);
}

TEST(SequenceArrayTest, InsertOutOfBoundsThrows) {
    int items[] = {1, 2, 3};
    MutableArraySequence<int> sequence(items, 3);
    EXPECT_THROW(sequence.insert_at(5, 4), std::out_of_range); // Позиции только 0..3
}

// Immutable-вставки создают новые последовательности, не меняя исходные элементы и длину.
TEST(SequenceArrayTest, ImmutableOperationsCreateNewObject) {
    int items[] = {1, 3};
    ImmutableArraySequence<int> original(items, 2);
    auto added = original.append(4);
    auto prepended = original.prepend(0);
    auto inserted = original.insert_at(2, 1);

    EXPECT_EQ(original.get_count(), 2u);
    EXPECT_EQ(added->get(2), 4);
    EXPECT_EQ(prepended->get(0), 0);
    EXPECT_EQ(inserted->get(1), 2);
}

// append(get(0)) получает ссылку внутрь буфера: если сначала перевыделить буфер,
// а потом читать значение, ссылка уже висит.
TEST(SequenceArrayTest, AppendOwnElementSurvivesResize) {
    int items[] = {1, 2, 3, 4};
    MutableArraySequence<int> sequence(items, 4);
    sequence.append(sequence.get(0));
    EXPECT_EQ(sequence.get_last(), 1);
}

// То же для сдвига элементов: ссылка указывает на ячейку, которую вставка перезаписывает.
TEST(SequenceArrayTest, InsertOwnElementSurvivesShifting) {
    int items[] = {1, 2, 3, 4};
    MutableArraySequence<int> sequence(items, 4);
    sequence.append(5); // Буфер вырос до 8: дальше вставки сдвигают, а не перевыделяют
    sequence.prepend(sequence.get(2));
    EXPECT_EQ(sequence.get_first(), 3);
    sequence.insert_at(sequence.get_last(), 1);
    EXPECT_EQ(sequence.get(1), 5);
}

TEST(SequenceArrayTest, Map) {
    int items[] = {1, 2, 3, 4};
    MutableArraySequence<int> sequence(items, 4);
    auto mapped = sequence.map(double_value);
    EXPECT_EQ(mapped->get_count(), 4u);
    EXPECT_EQ(mapped->get_last(), 8);
    EXPECT_EQ(sequence.get_last(), 4); // Исходная не изменилась
}

TEST(SequenceArrayTest, Where) {
    int items[] = {1, 2, 3, 4};
    MutableArraySequence<int> sequence(items, 4);
    auto filtered = sequence.where(is_even);
    EXPECT_EQ(filtered->get_count(), 2u);
    EXPECT_EQ(filtered->get_first(), 2);
}

TEST(SequenceArrayTest, ReduceStartsFromInitial) {
    int items[] = {1, 2, 3, 4};
    MutableArraySequence<int> sequence(items, 4);
    EXPECT_EQ(sequence.reduce(add_values, 10), 20); // 10 + 1 + 2 + 3 + 4
}

TEST(SequenceArrayTest, SubSequenceAndConcat) {
    int items[] = {1, 2, 3, 4};
    MutableArraySequence<int> sequence(items, 4);
    auto sub = sequence.get_sub_sequence(1, 2); // Индексы 1..2 включительно
    EXPECT_EQ(sub->get_count(), 2u);
    EXPECT_EQ(sub->get_last(), 3);

    auto joined = sequence.concat(*sub);
    EXPECT_EQ(joined->get_count(), 6u);
    EXPECT_EQ(sequence.get_count(), 4u);
}

// slice(1, 2, {2, 4}): с индекса 1 убрать 2 элемента и вставить {2, 4} -> {1, 2, 4, 4}
TEST(SequenceArrayTest, SliceWithReplacement) {
    int items[] = {1, 2, 3, 4};
    MutableArraySequence<int> sequence(items, 4);
    auto filtered = sequence.where(is_even);
    auto sliced = sequence.slice(1, 2, *filtered);
    EXPECT_EQ(sliced->get_count(), 4u);
    EXPECT_EQ(sliced->get_first(), 1);
    EXPECT_EQ(sliced->get_last(), 4);
}

// Изменение массива инвалидирует перечислитель до обращения к данным.
TEST(SequenceArrayTest, EnumeratorDetectsResize) {
    MutableArraySequence<int> array;
    array.append(1);
    auto iter = array.get_enumerator();
    EXPECT_TRUE(iter->move_next());
    array.append(2);
    EXPECT_THROW(iter->get_current(), std::logic_error);
}

TEST(SequenceArrayTest, EnumeratorDetectsOwnerDestruction) {
    UnqPtr<IEnumerator<int>> after_death;
    {
        MutableArraySequence<int> array;
        array.append(1);
        after_death = array.get_enumerator();
    }
    EXPECT_THROW(after_death->move_next(), std::logic_error);
}

// ---------------------------------------------------------------- ListSequence

// try_get* не требуют конструктора по умолчанию: optional пуст, а не хранит пустой объект.
TEST(SequenceListTest, TryGetWorksWithDeletedDefaultConstructor) {
    MutableListSequence<NoDefault> sequence;
    EXPECT_FALSE(sequence.try_get_first().has_value());
    EXPECT_FALSE(sequence.try_get_last().has_value());
    EXPECT_FALSE(sequence.try_get(0).has_value());

    sequence.append(NoDefault(17));
    EXPECT_EQ(sequence.try_get_first()->value, 17);
    EXPECT_EQ(sequence.try_get_last()->value, 17);
    EXPECT_EQ(sequence.try_get(0)->value, 17);
    EXPECT_FALSE(sequence.try_get(1).has_value());
}

// Mutable-список меняется на месте, вызовы идут цепочкой.
TEST(SequenceListTest, MutableOperations) {
    MutableListSequence<int> mutable_list;
    mutable_list.append(2).prepend(1).insert_at(3, 2);
    EXPECT_EQ(mutable_list.get_count(), 3u);
    EXPECT_EQ(mutable_list.get(1), 2);
}

// Immutable-список сохраняет каждую прежнюю версию.
TEST(SequenceListTest, ImmutableKeepsEveryVersion) {
    ImmutableListSequence<int> immutable_list;
    auto first = immutable_list.append(1);
    auto second = first->append(2);
    EXPECT_EQ(immutable_list.get_count(), 0u);
    EXPECT_EQ(first->get_count(), 1u);
    EXPECT_EQ(second->get_count(), 2u);
    EXPECT_EQ(second->get_last(), 2);
}

// Wrapper не перезапускает завершённый обход списка; reset() явно возвращает его к началу.
TEST(SequenceListTest, EnumeratorCanResetAndStaysExhausted) {
    int items[] = {5, 6};
    MutableListSequence<int> sequence(items, 2);
    EnumeratorWrapper<int> iter(sequence.get_enumerator());

    EXPECT_TRUE(iter.move_next());
    EXPECT_EQ(iter.get_current(), 5);
    EXPECT_TRUE(iter.move_next());
    EXPECT_EQ(iter.get_current(), 6);
    EXPECT_FALSE(iter.move_next());
    EXPECT_FALSE(iter.move_next());
    EXPECT_THROW(iter.get_current(), std::logic_error);

    iter.reset();
    EXPECT_TRUE(iter.move_next());
    EXPECT_EQ(iter.get_current(), 5);
}

// После изменения списка перечислитель бросает, а не читает старые узлы.
TEST(SequenceListTest, EnumeratorDetectsMutation) {
    MutableListSequence<int> list;
    list.append(1);
    auto iter = list.get_enumerator();
    EXPECT_TRUE(iter->move_next());
    list.append(2);
    EXPECT_THROW(iter->get_current(), std::logic_error);
    EXPECT_THROW(iter->move_next(), std::logic_error);
}

TEST(SequenceListTest, EnumeratorDetectsOwnerDestruction) {
    UnqPtr<IEnumerator<int>> after_death;
    {
        MutableListSequence<int> list;
        list.append(1);
        after_death = list.get_enumerator();
    }
    EXPECT_THROW(after_death->move_next(), std::logic_error);
}

// Перемещение списка переносит узлы, но инвалидирует перечислитель исходного владельца.
TEST(SequenceListTest, EnumeratorDetectsOwnerMove) {
    int items[] = {1, 2};
    LinkedList<int> source(items, 2);
    auto iter = source.get_enumerator();
    LinkedList<int> target(std::move(source));

    EXPECT_EQ(target.get_length(), 2u);
    EXPECT_THROW(iter->move_next(), std::logic_error);
}

TEST(SequenceListTest, ClearInvalidatesEnumerator) {
    int items[] = {1, 2};
    LinkedList<int> list(items, 2);
    auto iter = list.get_enumerator();
    list.clear();
    EXPECT_EQ(list.get_length(), 0u);
    EXPECT_THROW(iter->move_next(), std::logic_error);
}

// Хвост после clear() сброшен: иначе append дописал бы к удалённому узлу.
TEST(SequenceListTest, ClearResetsTail) {
    int items[] = {1, 2};
    LinkedList<int> list(items, 2);
    list.clear();
    EXPECT_THROW(list.get_last(), std::out_of_range);

    list.append(3);
    EXPECT_EQ(list.get_first(), 3);
    EXPECT_EQ(list.get_last(), 3);
}

// Clone() и map() списка создают независимые результаты, не растущие вместе с оригиналом.
TEST(SequenceListTest, CopyAndMapResultAreIndependent) {
    int items[] = {1, 2, 3};
    MutableListSequence<int> original(items, 3);
    auto copy = original.Clone();
    auto mapped = original.map(double_value);

    original.append(4);
    EXPECT_EQ(copy->get_count(), 3u);
    EXPECT_EQ(mapped->get_count(), 3u);
    EXPECT_EQ(mapped->get_last(), 6);
    EXPECT_EQ(original.get_count(), 4u);
}

// Исключение внутри map() освобождает частично построенный результат и сохраняет исходный список.
TEST(SequenceListTest, FailedMapReleasesPartialResult) {
    EXPECT_EQ(Counted::alive, 0);
    Counted items[] = {Counted(1), Counted(2)};
    MutableListSequence<Counted> sequence(items, 2);
    int before = Counted::alive;

    EXPECT_THROW(sequence.map(fail_on_two), std::runtime_error);
    EXPECT_EQ(Counted::alive, before);
    EXPECT_EQ(sequence.get_count(), 2u);
    EXPECT_EQ(sequence.get_first().value, 1);
}

// Рекурсивный деструктор узлов на 50 000 элементах переполнил бы стек.
TEST(SequenceListTest, LongListIsReleasedIteratively) {
    LinkedList<int> list;
    for (int index = 0; index < 50000; index++) list.append(index);
    EXPECT_EQ(list.get_length(), 50000u);
    EXPECT_EQ(list.get_last(), 49999);
} // Проверяется настоящий деструктор, а не только clear().

TEST(SequenceListTest, DestructorDestroysEveryElement) {
    EXPECT_EQ(Counted::alive, 0);
    {
        Counted item(7);
        LinkedList<Counted> list;
        for (int index = 0; index < 1000; index++) list.append(item);
        EXPECT_EQ(Counted::alive, 1001); // item и 1000 копий в узлах
    }
    EXPECT_EQ(Counted::alive, 0);
}

// ---------------------------------------------------------------- Неверные аргументы

TEST(SequenceTest, NullFunctionRejected) {
    MutableArraySequence<int> sequence;
    EXPECT_THROW(sequence.map(nullptr), std::invalid_argument);
    EXPECT_THROW(sequence.where(nullptr), std::invalid_argument);
}

TEST(SequenceTest, BadIndicesRejected) {
    MutableArraySequence<int> empty;
    EXPECT_THROW(empty.get_sub_sequence(0, 0), std::out_of_range);

    int items[] = {1, 2};
    MutableListSequence<int> list(items, 2);
    EXPECT_THROW(list.slice(0, -1), std::invalid_argument); // Отрицательная длина
    EXPECT_THROW(list.slice(2, 0), std::out_of_range);
    EXPECT_THROW(list.insert_at(3, 4), std::out_of_range);
    EXPECT_THROW(list.get(static_cast<size_t>(-1)), std::out_of_range);
}

// size_t(-1) элементов не выделить: должно быть исключение, а не переполнение при умножении.
TEST(SequenceTest, ImpossibleLengthRejected) {
    int items[] = {1, 2};
    EXPECT_THROW(DynamicArray<int>(static_cast<size_t>(-1)), std::length_error);
    EXPECT_THROW(LinkedList<int>(items, static_cast<size_t>(-1)), std::length_error);
}

// Отрицательный индекс slice считается с конца: -1 - последний элемент.
TEST(SequenceTest, SliceFromEnd) {
    int items[] = {1, 2};
    MutableListSequence<int> list(items, 2);
    auto without_last = list.slice(-1, 1);
    EXPECT_EQ(without_last->get_count(), 1u);
    EXPECT_EQ(without_last->get_first(), 1);
}

// ---------------------------------------------------------------- Отказы выделения памяти

// Повторяет action, заставляя отказать 1-е, 2-е, ... выделение памяти, пока action не пройдёт
// целиком. После каждого отказа проверяются утечки и то, что исходные данные не испорчены.
// Возвращает, сколько отказов удалось вызвать.
template <class Action, class Check>
int fail_every_allocation(Action action, Check check) {
    for (size_t failing = 1;; failing++) {
        size_t live = alloc_snapshot().live();
        AllocFailureScope failure(failing);
        try {
            action();
            return static_cast<int>(failing) - 1;
        } catch (const std::bad_alloc&) {
            EXPECT_EQ(alloc_snapshot().live(), live) << "Leak after failure " << failing;
            check();
        }
    }
}

// Копирование списка при отказе на любом узле не оставляет полусозданных узлов.
TEST(FaultInjectionTest, ListCopySurvivesEveryAllocationFailure) {
    Counted items[] = {Counted(1), Counted(2), Counted(3), Counted(4)};
    MutableListSequence<Counted> original(items, 4);
    int alive = Counted::alive;

    int failures = fail_every_allocation(
        [&] {
            auto copy = original.Clone();
            EXPECT_EQ(copy->get_count(), 4u);
        },
        [&] {
            EXPECT_EQ(Counted::alive, alive);
            EXPECT_EQ(original.get_count(), 4u);
            EXPECT_EQ(original.get_last().value, 4);
        });
    EXPECT_GE(failures, 5); // копия, её состояние итераторов и четыре узла
}

// Расширение массива при нехватке памяти оставляет прежние элементы (строгая гарантия).
TEST(FaultInjectionTest, ArrayAppendKeepsContentsWhenResizeFails) {
    int items[] = {1, 2, 3, 4};
    MutableArraySequence<int> sequence(items, 4);

    fail_every_allocation(
        [&] { sequence.append(5); },
        [&] {
            ASSERT_EQ(sequence.get_count(), 4u);
            for (size_t index = 0; index < 4; index++) {
                EXPECT_EQ(sequence.get(index), static_cast<int>(index) + 1);
            }
        });
    EXPECT_EQ(sequence.get_count(), 5u);
    EXPECT_EQ(sequence.get_last(), 5);
}

// map() при отказе на любом шаге освобождает частичный результат.
TEST(FaultInjectionTest, MapSurvivesEveryAllocationFailure) {
    int items[] = {1, 2, 3, 4, 5};
    MutableListSequence<int> sequence(items, 5);

    fail_every_allocation(
        [&] {
            auto mapped = sequence.map(double_value);
            EXPECT_EQ(mapped->get_last(), 10);
        },
        [&] { EXPECT_EQ(sequence.get_count(), 5u); });
}

// Вставка в середину списка при отказе не меняет связи узлов.
TEST(FaultInjectionTest, ListInsertKeepsLinksWhenNodeAllocationFails) {
    int items[] = {1, 3};
    LinkedList<int> list(items, 2);
    {
        AllocFailureScope failure(1);
        EXPECT_THROW(list.insert_at(2, 1), std::bad_alloc);
    }
    EXPECT_EQ(list.get_length(), 2u);
    EXPECT_EQ(list.get(0), 1);
    EXPECT_EQ(list.get(1), 3);
    list.insert_at(2, 1);
    EXPECT_EQ(list.get(1), 2);
}

// ---------------------------------------------------------------- Пустые и граничные случаи

// Операции над пустыми последовательностями дают пустые результаты, reduce — начальное значение.
TEST(SequenceTest, EmptySequenceOperations) {
    MutableListSequence<int> empty_list;
    MutableArraySequence<int> empty_array;
    EXPECT_EQ(empty_list.map(double_value)->get_count(), 0u);
    EXPECT_EQ(empty_array.where(is_even)->get_count(), 0u);
    EXPECT_EQ(empty_list.concat(empty_array)->get_count(), 0u);
    EXPECT_EQ(empty_array.reduce(add_values, 7), 7);
    EXPECT_FALSE(empty_list.get_enumerator()->move_next());
}

TEST(SequenceTest, EmptySequenceAccessThrows) {
    MutableListSequence<int> empty_list;
    MutableArraySequence<int> empty_array;
    EXPECT_THROW(empty_list.get_first(), std::out_of_range);
    EXPECT_THROW(empty_array.get_last(), std::out_of_range);
    EXPECT_THROW(empty_array.slice(0, 0), std::out_of_range);
}

// Границы get_sub_sequence: весь диапазон, один элемент, последний индекс.
TEST(SequenceTest, SubSequenceBoundaries) {
    int items[] = {1, 2, 3};
    MutableArraySequence<int> sequence(items, 3);

    EXPECT_EQ(sequence.get_sub_sequence(0, 2)->get_count(), 3u);
    auto single = sequence.get_sub_sequence(1, 1);
    EXPECT_EQ(single->get_count(), 1u);
    EXPECT_EQ(single->get_first(), 2);
    EXPECT_EQ(sequence.get_sub_sequence(2, 2)->get_first(), 3);
    EXPECT_THROW(sequence.get_sub_sequence(0, 3), std::out_of_range); // Конец за пределами
    EXPECT_THROW(sequence.get_sub_sequence(2, 1), std::out_of_range); // Начало после конца
}
