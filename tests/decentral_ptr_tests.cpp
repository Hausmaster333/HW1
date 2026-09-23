#include "unq_ptr.h"
#include "shrd_ptr.h"
#include <gtest/gtest.h>
#include <stdexcept>
#include <type_traits>
#include <utility>

struct TrackedObject {
    static int alive;
    int value;

    TrackedObject(int value) : value(value) { alive++; }
    TrackedObject(const TrackedObject& other) : value(other.value) { alive++; }
    ~TrackedObject() { alive--; }
};

int TrackedObject::alive = 0;

struct FirstBase {
    int first = 10;
};

struct SecondBase {
    int second = 20;
};

struct Child : FirstBase, SecondBase {
    static int copies;
    static int destroyed;

    Child() = default;
    Child(const Child& other) : FirstBase(other), SecondBase(other) { copies++; }
    ~Child() { destroyed++; }
};

int Child::copies = 0;
int Child::destroyed = 0;

struct ThrowOnCopy {
    static int alive;

    ThrowOnCopy() { alive++; }
    ThrowOnCopy(const ThrowOnCopy&) { throw std::runtime_error("Copy failed"); }
    ~ThrowOnCopy() { alive--; }
};

int ThrowOnCopy::alive = 0;

struct ArrayElement {
    static int alive;
    int value = 0;

    ArrayElement() { alive++; }
    ArrayElement& operator=(const ArrayElement& other) {
        value = other.value;
        return *this;
    }
    ~ArrayElement() { alive--; }
};

int ArrayElement::alive = 0;

struct ThrowArrayElement {
    static int alive;
    static int assignments;

    ThrowArrayElement() { alive++; }
    ThrowArrayElement& operator=(const ThrowArrayElement&) {
        assignments++;
        if (assignments == 2) throw std::runtime_error("Array copy failed");
        return *this;
    }
    ~ThrowArrayElement() { alive--; }
};

int ThrowArrayElement::alive = 0;
int ThrowArrayElement::assignments = 0;

struct MoveOnlyObject {
    int value;

    MoveOnlyObject(int value) : value(value) {}
    MoveOnlyObject(const MoveOnlyObject&) = delete;
};

struct MoveOnlyArrayElement {
    int value = 0;
    MoveOnlyArrayElement() = default;
    MoveOnlyArrayElement(const MoveOnlyArrayElement&) = delete;
    MoveOnlyArrayElement& operator=(const MoveOnlyArrayElement&) = delete;
};

static_assert(!std::is_copy_constructible<UnqPtr<int>>::value, "UnqPtr must be move-only");
static_assert(!std::is_copy_assignable<UnqPtr<int>>::value, "UnqPtr must be move-only");
static_assert(!std::is_constructible<UnqPtr<Child>, UnqPtr<SecondBase>&&>::value,
              "Downcast must be rejected");
static_assert(!std::is_constructible<UnqPtr<SecondBase[]>, UnqPtr<Child[]>&&>::value,
              "Arrays must not be covariant");
static_assert(!std::is_constructible<UnqPtr<int>, int*>::value,
              "Raw pointer ownership must not be public");

TEST(UnqPtrTest, EmptyAndMove) {
    UnqPtr<int> empty;
    EXPECT_FALSE(empty);
    EXPECT_EQ(empty.get(), nullptr);
    EXPECT_EQ(empty.descriptor_count(), 0u);
    EXPECT_THROW(*empty, std::logic_error);
    EXPECT_FALSE(empty.clone());

    auto first = UnqPtr<int>::make(5);
    UnqPtr<int> second = std::move(first);
    EXPECT_FALSE(first);
    EXPECT_EQ(*second, 5);
    UnqPtr<int>* alias = &second;
    second = std::move(*alias);
    EXPECT_EQ(*second, 5);
}

TEST(UnqPtrTest, CloneIsIndependent) {
    EXPECT_EQ(TrackedObject::alive, 0);
    auto owner = UnqPtr<TrackedObject>::make(10);
    auto copy = owner.clone();

    EXPECT_NE(owner.get(), copy.get());
    EXPECT_EQ(TrackedObject::alive, 2);
    copy->value = 20;
    EXPECT_EQ(owner->value, 10);
    EXPECT_EQ(copy->value, 20);
    owner.reset();
    EXPECT_EQ(owner.get(), nullptr);
    EXPECT_EQ(TrackedObject::alive, 1);
    copy.reset();
    EXPECT_EQ(TrackedObject::alive, 0);
}

TEST(UnqPtrTest, MoveOnlyObjectCanBeOwnedButNotCloned) {
    auto owner = UnqPtr<MoveOnlyObject>::make(13);
    EXPECT_EQ(owner->value, 13);
    EXPECT_THROW(owner.clone(), std::logic_error);
    EXPECT_EQ(owner->value, 13);
}

TEST(ShrdPtrTest, DescriptorsOutliveOwner) {
    EXPECT_EQ(TrackedObject::alive, 0);
    auto owner = UnqPtr<TrackedObject>::make(1);
    auto first = owner.share();
    auto second = first;

    EXPECT_EQ(owner.descriptor_count(), 2u);
    EXPECT_EQ(first.use_count(), 2u);
    owner.reset();
    EXPECT_EQ(TrackedObject::alive, 1);

    first->value = 7;
    EXPECT_EQ(second->value, 7);
    first.reset();
    EXPECT_EQ(TrackedObject::alive, 1);
    second.reset();
    EXPECT_EQ(TrackedObject::alive, 0);
}

TEST(ShrdPtrTest, OldDescriptorsStayWithOldObject) {
    EXPECT_EQ(TrackedObject::alive, 0);
    auto owner = UnqPtr<TrackedObject>::make(1);
    auto old = owner.share();

    owner = UnqPtr<TrackedObject>::make(2);
    EXPECT_EQ(old->value, 1);
    EXPECT_EQ(owner->value, 2);
    EXPECT_EQ(TrackedObject::alive, 2);

    old.reset();
    EXPECT_EQ(TrackedObject::alive, 1);
    owner.reset();
    EXPECT_EQ(TrackedObject::alive, 0);
}

TEST(ShrdPtrTest, MoveOwnerDoesNotBreakDescriptors) {
    auto owner = UnqPtr<int>::make(11);
    auto descriptor = owner.share();
    UnqPtr<int> moved = std::move(owner);

    EXPECT_FALSE(owner);
    EXPECT_EQ(moved.descriptor_count(), 1u);
    EXPECT_EQ(*descriptor, 11);
    moved.reset();
    EXPECT_EQ(*descriptor, 11);
}

TEST(ShrdPtrTest, AssignmentReleasesPreviousObject) {
    EXPECT_EQ(TrackedObject::alive, 0);
    auto first_owner = UnqPtr<TrackedObject>::make(1);
    auto second_owner = UnqPtr<TrackedObject>::make(2);
    auto first = first_owner.share();
    auto second = second_owner.share();

    first = second;
    EXPECT_EQ(first->value, 2);
    EXPECT_EQ(second.use_count(), 2u);
    first_owner.reset();
    EXPECT_EQ(TrackedObject::alive, 1);

    second_owner.reset();
    second.reset();
    EXPECT_EQ(TrackedObject::alive, 1);
    first.reset();
    EXPECT_EQ(TrackedObject::alive, 0);
}

TEST(UnqPtrTest, CloneAfterUpcastPreservesChild) {
    Child::copies = 0;
    Child::destroyed = 0;
    auto child = UnqPtr<Child>::make();
    SecondBase* expected = static_cast<SecondBase*>(child.get());

    UnqPtr<SecondBase> base = std::move(child);
    EXPECT_FALSE(child);
    EXPECT_EQ(base.get(), expected);
    auto copy = base.clone();
    EXPECT_EQ(Child::copies, 1);
    EXPECT_NE(copy.get(), base.get());

    copy->second = 99;
    EXPECT_EQ(base->second, 20);
    EXPECT_EQ(copy->second, 99);
    base.reset();
    copy.reset();
    EXPECT_EQ(Child::destroyed, 2);
}

TEST(ShrdPtrTest, UpcastAdjustsAddressAndSurvivesOwner) {
    Child::destroyed = 0;
    auto owner = UnqPtr<Child>::make();
    SecondBase* expected = static_cast<SecondBase*>(owner.get());

    ShrdPtr<SecondBase> base(owner);
    auto child_descriptor = owner.share();
    ShrdPtr<SecondBase> second = child_descriptor;
    EXPECT_EQ(base.get(), expected);
    EXPECT_EQ(second.get(), expected);
    EXPECT_EQ(base.use_count(), 3u);

    owner.reset();
    EXPECT_EQ(Child::destroyed, 0);
    base.reset();
    second.reset();
    child_descriptor.reset();
    EXPECT_EQ(Child::destroyed, 1);
}

TEST(UnqPtrTest, FailedCloneKeepsOriginal) {
    EXPECT_EQ(ThrowOnCopy::alive, 0);
    auto owner = UnqPtr<ThrowOnCopy>::make();
    EXPECT_THROW(owner.clone(), std::runtime_error);
    EXPECT_TRUE(owner);
    EXPECT_EQ(ThrowOnCopy::alive, 1);
    owner.reset();
    EXPECT_EQ(ThrowOnCopy::alive, 0);
}

TEST(UnqPtrArrayTest, CloneAndDescriptorLifetime) {
    EXPECT_EQ(ArrayElement::alive, 0);
    auto owner = UnqPtr<ArrayElement[]>::make_array(3);
    EXPECT_EQ(ArrayElement::alive, 3);
    owner[1].value = 42;

    auto copy = owner.clone();
    EXPECT_EQ(ArrayElement::alive, 6);
    EXPECT_EQ(copy[1].value, 42);
    copy[1].value = 7;
    EXPECT_EQ(owner[1].value, 42);

    auto descriptor = owner.share();
    auto second_descriptor = descriptor;
    owner.reset();
    EXPECT_EQ(descriptor[1].value, 42);
    EXPECT_EQ(descriptor.size(), 3u);
    EXPECT_THROW(descriptor[3], std::out_of_range);
    descriptor.reset();
    EXPECT_EQ(ArrayElement::alive, 6);
    second_descriptor.reset();
    EXPECT_EQ(ArrayElement::alive, 3);
    copy.reset();
    EXPECT_EQ(ArrayElement::alive, 0);
}

TEST(UnqPtrArrayTest, FailedCloneDestroysPartialCopy) {
    EXPECT_EQ(ThrowArrayElement::alive, 0);
    ThrowArrayElement::assignments = 0;
    auto owner = UnqPtr<ThrowArrayElement[]>::make_array(3);
    EXPECT_THROW(owner.clone(), std::runtime_error);
    EXPECT_EQ(ThrowArrayElement::alive, 3);
    EXPECT_TRUE(owner);
    owner.reset();
    EXPECT_EQ(ThrowArrayElement::alive, 0);
}

TEST(UnqPtrArrayTest, EmptyArray) {
    auto owner = UnqPtr<int[]>::make_array(0);
    EXPECT_FALSE(owner);
    EXPECT_EQ(owner.size(), 0u);
    EXPECT_THROW(owner[0], std::out_of_range);
    EXPECT_FALSE(owner.share());
}

TEST(UnqPtrArrayTest, MoveOnlyElementsCanBeOwnedButNotCloned) {
    auto owner = UnqPtr<MoveOnlyArrayElement[]>::make_array(2);
    owner[1].value = 19;
    EXPECT_THROW(owner.clone(), std::logic_error);
    EXPECT_EQ(owner[1].value, 19);
}
