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
static_assert(!std::is_constructible<UnqPtr<Child>, UnqPtr<SecondBase>&&>::value, "Downcast must be rejected");
static_assert(!std::is_constructible<UnqPtr<SecondBase[]>, UnqPtr<Child[]>&&>::value, "Arrays must not be covariant");
static_assert(!std::is_constructible<UnqPtr<int>, int*>::value, "Raw pointer ownership must not be public");
static_assert(!std::is_constructible<ShrdPtr<int>, UnqPtr<int>&>::value, "UnqPtr must be moved to transfer ownership");
static_assert(std::is_constructible<ShrdPtr<int>, UnqPtr<int>&&>::value, "ShrdPtr must accept ownership from UnqPtr");
static_assert(std::is_constructible<ShrdPtr<SecondBase>, UnqPtr<Child>&&>::value, "Derived object must be transferable to a base pointer");
static_assert(!std::is_constructible<ShrdPtr<Child>, UnqPtr<SecondBase>&&>::value, "Downcast must be rejected");
static_assert(!std::is_constructible<UnqPtr<int>, ShrdPtr<int>&&>::value, "Moving ShrdPtr must not silently create UnqPtr");

TEST(UnqPtrTest, EmptyAndMove) {
    UnqPtr<int> empty;
    EXPECT_FALSE(empty);
    EXPECT_EQ(empty.get(), nullptr);
    EXPECT_THROW(*empty, std::logic_error);
    EXPECT_FALSE(empty.clone());

    auto first = UnqPtr<int>::make(5);
    UnqPtr<int> second = std::move(first);
    EXPECT_FALSE(first);
    EXPECT_EQ(*second, 5);
    UnqPtr<int>* alias = &second;
    second = std::move(*alias);
    EXPECT_EQ(*second, 5);
    auto copy = second.clone();
    EXPECT_EQ(*copy, 5);
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

TEST(ShrdPtrTest, CopiesShareOneObject) {
    EXPECT_EQ(TrackedObject::alive, 0);
    auto first = ShrdPtr<TrackedObject>::make(1);
    auto second = first;

    EXPECT_EQ(first.use_count(), 2u);
    EXPECT_EQ(second.use_count(), 2u);
    first.reset();
    EXPECT_EQ(TrackedObject::alive, 1);

    second->value = 7;
    EXPECT_EQ(second->value, 7);
    second.reset();
    EXPECT_EQ(TrackedObject::alive, 0);
}

TEST(ShrdPtrTest, TakesOwnershipFromUnqPtr) {
    EXPECT_EQ(TrackedObject::alive, 0);
    auto unique = UnqPtr<TrackedObject>::make(17);
    TrackedObject* address = unique.get();

    ShrdPtr<TrackedObject> shared(std::move(unique));
    EXPECT_FALSE(unique);
    EXPECT_EQ(shared.get(), address);
    EXPECT_EQ(shared.use_count(), 1u);
    EXPECT_EQ(TrackedObject::alive, 1);

    auto second = shared;
    shared.reset();
    EXPECT_EQ(TrackedObject::alive, 1);
    second.reset();
    EXPECT_EQ(TrackedObject::alive, 0);
}

TEST(ShrdPtrTest, TransferPreservesDerivedDeleterAndBaseAddress) {
    Child::destroyed = 0;
    auto child = UnqPtr<Child>::make();
    SecondBase* address = static_cast<SecondBase*>(child.get());

    ShrdPtr<SecondBase> shared(std::move(child));
    EXPECT_FALSE(child);
    EXPECT_EQ(shared.get(), address);
    shared.reset();
    EXPECT_EQ(Child::destroyed, 1);

    auto next_child = UnqPtr<Child>::make();
    UnqPtr<SecondBase> base(std::move(next_child));
    address = base.get();
    ShrdPtr<SecondBase> next_shared(std::move(base));
    EXPECT_FALSE(base);
    EXPECT_EQ(next_shared.get(), address);
    next_shared.reset();
    EXPECT_EQ(Child::destroyed, 2);
}

TEST(ShrdPtrTest, AssignmentKeepsOldObjectForOtherOwners) {
    EXPECT_EQ(TrackedObject::alive, 0);
    auto first = ShrdPtr<TrackedObject>::make(1);
    auto old = first;

    first = ShrdPtr<TrackedObject>::make(2);
    EXPECT_EQ(old->value, 1);
    EXPECT_EQ(first->value, 2);
    EXPECT_EQ(TrackedObject::alive, 2);

    old.reset();
    EXPECT_EQ(TrackedObject::alive, 1);
    first.reset();
    EXPECT_EQ(TrackedObject::alive, 0);
}

TEST(ShrdPtrTest, MoveDoesNotChangeUseCount) {
    auto first = ShrdPtr<int>::make(11);
    auto second = first;
    ShrdPtr<int> moved = std::move(first);

    EXPECT_FALSE(first);
    EXPECT_EQ(moved.use_count(), 2u);
    EXPECT_EQ(*second, 11);
    moved.reset();
    EXPECT_EQ(second.use_count(), 1u);
}

TEST(ShrdPtrTest, AssignmentReleasesPreviousObject) {
    EXPECT_EQ(TrackedObject::alive, 0);
    auto first = ShrdPtr<TrackedObject>::make(1);
    auto second = ShrdPtr<TrackedObject>::make(2);

    first = second;
    EXPECT_EQ(first->value, 2);
    EXPECT_EQ(second.use_count(), 2u);
    EXPECT_EQ(TrackedObject::alive, 1);

    second.reset();
    EXPECT_EQ(TrackedObject::alive, 1);
    first.reset();
    EXPECT_EQ(TrackedObject::alive, 0);
}

TEST(UnqPtrTest, CloneAfterUpcastIsRejectedWithoutSlicing) {
    Child::copies = 0;
    Child::destroyed = 0;
    auto child = UnqPtr<Child>::make();
    SecondBase* expected = static_cast<SecondBase*>(child.get());

    UnqPtr<SecondBase> base = std::move(child);
    EXPECT_FALSE(child);
    EXPECT_EQ(base.get(), expected);
    EXPECT_THROW(base.clone(), std::logic_error);
    EXPECT_EQ(Child::copies, 0);

    base.reset();
    EXPECT_EQ(Child::destroyed, 1);
}

TEST(ShrdPtrTest, UpcastAdjustsAddressAndSharesOwnership) {
    Child::destroyed = 0;
    auto child = ShrdPtr<Child>::make();
    SecondBase* expected = static_cast<SecondBase*>(child.get());

    ShrdPtr<SecondBase> base(child);
    ShrdPtr<SecondBase> second = child;
    EXPECT_EQ(base.get(), expected);
    EXPECT_EQ(second.get(), expected);
    EXPECT_EQ(base.use_count(), 3u);

    child.reset();
    EXPECT_EQ(Child::destroyed, 0);
    base.reset();
    second.reset();
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

TEST(UnqPtrArrayTest, CloneIsIndependent) {
    EXPECT_EQ(ArrayElement::alive, 0);
    auto owner = UnqPtr<ArrayElement[]>::make_array(3);
    EXPECT_EQ(ArrayElement::alive, 3);
    owner[1].value = 42;

    auto copy = owner.clone();
    EXPECT_EQ(ArrayElement::alive, 6);
    EXPECT_EQ(copy[1].value, 42);
    copy[1].value = 7;
    EXPECT_EQ(owner[1].value, 42);

    owner.reset();
    EXPECT_EQ(ArrayElement::alive, 3);
    copy.reset();
    EXPECT_EQ(ArrayElement::alive, 0);
}

TEST(ShrdPtrArrayTest, LastOwnerDeletesArray) {
    EXPECT_EQ(ArrayElement::alive, 0);
    auto first = ShrdPtr<ArrayElement[]>::make_array(3);
    first[1].value = 42;
    auto second = first;

    EXPECT_EQ(first.use_count(), 2u);
    first.reset();
    EXPECT_EQ(ArrayElement::alive, 3);
    EXPECT_EQ(second.size(), 3u);
    EXPECT_EQ(second[1].value, 42);
    EXPECT_THROW(second[3], std::out_of_range);
    second.reset();
    EXPECT_EQ(ArrayElement::alive, 0);
}

TEST(ShrdPtrArrayTest, TakesOwnershipFromUnqPtr) {
    EXPECT_EQ(ArrayElement::alive, 0);
    auto unique = UnqPtr<ArrayElement[]>::make_array(3);
    auto* address = unique.get();
    unique[1].value = 42;

    ShrdPtr<ArrayElement[]> shared(std::move(unique));
    EXPECT_FALSE(unique);
    EXPECT_EQ(unique.size(), 0u);
    EXPECT_EQ(shared.get(), address);
    EXPECT_EQ(shared.size(), 3u);
    EXPECT_EQ(shared[1].value, 42);
    shared.reset();
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
    auto shared = ShrdPtr<int[]>::make_array(0);
    EXPECT_FALSE(shared);
    EXPECT_EQ(shared.size(), 0u);
    EXPECT_EQ(shared.use_count(), 0u);
    ShrdPtr<int[]> moved(std::move(owner));
    EXPECT_FALSE(moved);
    EXPECT_EQ(moved.use_count(), 0u);
}

TEST(UnqPtrArrayTest, MoveOnlyElementsCanBeOwnedButNotCloned) {
    auto owner = UnqPtr<MoveOnlyArrayElement[]>::make_array(2);
    owner[1].value = 19;
    EXPECT_THROW(owner.clone(), std::logic_error);
    EXPECT_EQ(owner[1].value, 19);
}
