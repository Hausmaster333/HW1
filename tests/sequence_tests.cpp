#include "sequence/sequence.h"
#include <gtest/gtest.h>
#include <stdexcept>
#include <type_traits>
#include <utility>

int double_value(const int& value) { return value * 2; }

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

Counted fail_on_two(const Counted& item) {
    if (item.value == 2) throw std::runtime_error("Map failed");
    return item;
}

// ======================= Владение

// Буфер лежит в UnqPtr<T[]>: копия массива должна получить свой буфер, а не тот же адрес
TEST(DynamicArrayTest, CopyIsIndependent) {
    int items[] = {1, 2, 3};
    DynamicArray<int> original(items, 3);
    DynamicArray<int> copy(original);
    copy.set(1, 9);
    EXPECT_EQ(original.get(1), 2);
}

// Clone и map не делят их с оригиналом
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

// Immutable-вставки создают новые последовательности, не меняя исходную
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

// ======================= Удаление

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

// Каждый узел владеет следующим через UnqPtr: рекурсивное удаление цепочки из 50 000 узлов переполнило бы стек
TEST(SequenceListTest, LongListIsReleasedIteratively) {
    LinkedList<int> list;
    for (int index = 0; index < 50000; index++) list.append(index);
    EXPECT_EQ(list.get_length(), 50000u);
    EXPECT_EQ(list.get_last(), 49999);
}

// Исключение внутри map() освобождает частично построенный результат и сохраняет исходный список
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

// Хвост после clear сброшен, иначе append дописал бы к удалённому узлу
TEST(SequenceListTest, ClearResetsTail) {
    int items[] = {1, 2};
    LinkedList<int> list(items, 2);
    list.clear();
    EXPECT_THROW(list.get_last(), std::out_of_range);

    list.append(3);
    EXPECT_EQ(list.get_first(), 3);
    EXPECT_EQ(list.get_last(), 3);
}

// ======================= Висячие ссылки

// append(get(0)) получает ссылку внутрь буфера: если сначала перевыделить буфер, а потом читать значение, ссылка уже висит
TEST(SequenceArrayTest, AppendOwnElementSurvivesResize) {
    int items[] = {1, 2, 3, 4};
    MutableArraySequence<int> sequence(items, 4);
    sequence.append(sequence.get(0));
    EXPECT_EQ(sequence.get_last(), 1);
}

// То же для сдвига элементов: ссылка указывает на ячейку, которую вставка перезаписывает
TEST(SequenceArrayTest, InsertOwnElementSurvivesShifting) {
    int items[] = {1, 2, 3, 4};
    MutableArraySequence<int> sequence(items, 4);
    sequence.append(5); // Буфер вырос до 8: дальше вставки сдвигают, а не перевыделяют
    sequence.prepend(sequence.get(2));
    EXPECT_EQ(sequence.get_first(), 3);
    sequence.insert_at(sequence.get_last(), 1);
    EXPECT_EQ(sequence.get(1), 5);
}

// ======================= Enumerators

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

// Перемещение списка переносит узлы, но инвалидирует перечислитель исходного владельца
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
