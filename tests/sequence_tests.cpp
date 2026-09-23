#include "sequence/sequence.h"
#include <gtest/gtest.h>
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

TEST(DynamicArrayTest, CopyAndResizeAreIndependent) {
    int items[] = {1, 2, 3};
    DynamicArray<int> original(items, 3);
    DynamicArray<int> copy(original);
    copy.set(1, 9);
    EXPECT_EQ(original.get(1), 2);

    copy.resize(5);
    EXPECT_EQ(copy.get_size(), 5u);
    EXPECT_EQ(copy.get(1), 9);
    EXPECT_EQ(copy.get(4), 0);
    EXPECT_EQ(original.get_size(), 3u);
}

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

TEST(SequenceArrayTest, TryGetReturnsEmptyOutsideBounds) {
    MutableArraySequence<int> sequence;
    EXPECT_FALSE(sequence.try_get_first().has_value());
    EXPECT_FALSE(sequence.try_get_last().has_value());
    EXPECT_FALSE(sequence.try_get(0).has_value());

    sequence.append(5);
    EXPECT_EQ(sequence.try_get_first().value(), 5);
    EXPECT_EQ(sequence.try_get_last().value(), 5);
    EXPECT_EQ(sequence.try_get(0).value(), 5);
    EXPECT_FALSE(sequence.try_get(1).has_value());
}

TEST(SequenceArrayTest, MutableOperationsKeepSameObject) {
    MutableArraySequence<int> sequence;
    auto* address = &sequence;
    EXPECT_EQ(&sequence.append(2), address);
    sequence.prepend(1);
    sequence.insert_at(3, 2);
    EXPECT_EQ(sequence.get_count(), 3u);
    EXPECT_EQ(sequence.get_first(), 1);
    EXPECT_EQ(sequence.get_last(), 3);

    sequence.chain_append(4).chain_prepend(0);
    EXPECT_EQ(sequence.get_count(), 5u);
    EXPECT_EQ(sequence.get(0), 0);
    EXPECT_EQ(sequence.get(4), 4);
    EXPECT_THROW(sequence.insert_at(5, 6), std::out_of_range);
}

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
    EXPECT_NE(added.get(), &original);
}

TEST(SequenceArrayTest, InsertingOwnElementSurvivesResizeAndShifting) {
    int items[] = {1, 2, 3, 4};
    MutableArraySequence<int> sequence(items, 4);
    sequence.append(sequence.get(0)); // расширяет буфер
    EXPECT_EQ(sequence.get_last(), 1);

    sequence.prepend(sequence.get(2)); // сдвигает элементы
    EXPECT_EQ(sequence.get_first(), 3);
    sequence.insert_at(sequence.get_last(), 1);
    EXPECT_EQ(sequence.get(1), 1);
}

TEST(SequenceArrayTest, ProducingOperationsOwnTheirResults) {
    int items[] = {1, 2, 3, 4};
    MutableArraySequence<int> sequence(items, 4);
    auto mapped = sequence.map(double_value);
    auto filtered = sequence.where(is_even);
    auto sub = sequence.get_sub_sequence(1, 2);
    auto joined = sequence.concat(filtered.get());
    auto sliced = sequence.slice(1, 2, filtered.get());

    EXPECT_EQ(mapped->get_count(), 4u);
    EXPECT_EQ(mapped->get_last(), 8);
    EXPECT_EQ(filtered->get_count(), 2u);
    EXPECT_EQ(filtered->get_first(), 2);
    EXPECT_EQ(sub->get_count(), 2u);
    EXPECT_EQ(sub->get_last(), 3);
    EXPECT_EQ(joined->get_count(), 6u);
    EXPECT_EQ(sliced->get_count(), 4u);
    EXPECT_EQ(sliced->get_first(), 1);
    EXPECT_EQ(sliced->get_last(), 4);
    EXPECT_EQ(sequence.reduce(add_values, 10), 20);
    EXPECT_EQ(sequence.get_count(), 4u);
}

TEST(SequenceListTest, MutableAndImmutableOperations) {
    MutableListSequence<int> mutable_list;
    mutable_list.append(2).prepend(1).insert_at(3, 2);
    EXPECT_EQ(mutable_list.get_count(), 3u);
    EXPECT_EQ(mutable_list.get(1), 2);

    ImmutableListSequence<int> immutable_list;
    auto first = immutable_list.append(1);
    auto second = first->append(2);
    EXPECT_EQ(immutable_list.get_count(), 0u);
    EXPECT_EQ(first->get_count(), 1u);
    EXPECT_EQ(second->get_count(), 2u);
    EXPECT_EQ(second->get_last(), 2);
}

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

TEST(SequenceListTest, EnumeratorDetectsMutationAndOwnerDestruction) {
    UnqPtr<IEnumerator<int>> after_death;
    {
        MutableListSequence<int> list;
        list.append(1);
        auto iter = list.get_enumerator();
        EXPECT_TRUE(iter->move_next());
        list.append(2);
        EXPECT_THROW(iter->get_current(), std::logic_error);
        EXPECT_THROW(iter->move_next(), std::logic_error);

        after_death = list.get_enumerator();
    }
    EXPECT_THROW(after_death->move_next(), std::logic_error);
}

TEST(SequenceListTest, EnumeratorDetectsOwnerMove) {
    int items[] = {1, 2};
    LinkedList<int> source(items, 2);
    auto iter = source.get_enumerator();
    LinkedList<int> target(std::move(source));

    EXPECT_EQ(target.get_length(), 2u);
    EXPECT_THROW(iter->move_next(), std::logic_error);
}

TEST(SequenceListTest, ClearInvalidatesEnumeratorAndResetsTail) {
    int items[] = {1, 2};
    LinkedList<int> list(items, 2);
    auto iter = list.get_enumerator();

    list.clear();
    EXPECT_EQ(list.get_length(), 0u);
    EXPECT_THROW(list.get_last(), std::out_of_range);
    EXPECT_THROW(iter->move_next(), std::logic_error);

    list.append(3);
    EXPECT_EQ(list.get_first(), 3);
    EXPECT_EQ(list.get_last(), 3);
}

TEST(SequenceArrayTest, EnumeratorDetectsResizeAndOwnerDestruction) {
    UnqPtr<IEnumerator<int>> after_death;
    {
        MutableArraySequence<int> array;
        array.append(1);
        auto iter = array.get_enumerator();
        EXPECT_TRUE(iter->move_next());
        array.append(2);
        EXPECT_THROW(iter->get_current(), std::logic_error);

        after_death = array.get_enumerator();
    }
    EXPECT_THROW(after_death->move_next(), std::logic_error);
}

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

TEST(SequenceListTest, LongListIsReleasedIteratively) {
    {
        LinkedList<int> list;
        for (int index = 0; index < 50000; index++) list.append(index);
        EXPECT_EQ(list.get_length(), 50000u);
        EXPECT_EQ(list.get_last(), 49999);
    } // Проверяется настоящий деструктор, а не только clear().

    EXPECT_EQ(Counted::alive, 0);
    {
        Counted item(7);
        LinkedList<Counted> list;
        for (int index = 0; index < 1000; index++) list.append(item);
        EXPECT_EQ(Counted::alive, 1001);
    }
    EXPECT_EQ(Counted::alive, 0);
}

TEST(SequenceTest, InvalidArguments) {
    MutableArraySequence<int> sequence;
    EXPECT_THROW(sequence.map(nullptr), std::invalid_argument);
    EXPECT_THROW(sequence.where(nullptr), std::invalid_argument);
    EXPECT_THROW(sequence.concat(nullptr), std::invalid_argument);
    EXPECT_THROW(sequence.get_sub_sequence(0, 0), std::out_of_range);

    int items[] = {1, 2};
    MutableListSequence<int> list(items, 2);
    EXPECT_THROW(list.slice(0, -1), std::invalid_argument);
    EXPECT_THROW(list.slice(2, 0), std::out_of_range);
    EXPECT_THROW(list.insert_at(3, 4), std::out_of_range);

    EXPECT_THROW(DynamicArray<int>(static_cast<size_t>(-1)), std::length_error);
    EXPECT_THROW(LinkedList<int>(items, static_cast<size_t>(-1)), std::length_error);
    EXPECT_THROW(list.get(static_cast<size_t>(-1)), std::out_of_range);

    auto without_last = list.slice(-1, 1);
    EXPECT_EQ(without_last->get_count(), 1u);
    EXPECT_EQ(without_last->get_first(), 1);
}
