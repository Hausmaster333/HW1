#include "unq_ptr.h"
#include "shrd_ptr.h"
#include "weak_ptr.h"
#include "support/alloc_counter.h"
#include <gtest/gtest.h>
#include <atomic>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
#include <memory>

struct Tracked {
    static int alive;
    int value;

    explicit Tracked(int value = 0) : value(value) { alive++; }
    Tracked(const Tracked& other) : value(other.value) { alive++; }
    ~Tracked() { alive--; }
};

int Tracked::alive = 0;

struct ThrowOnConstruct {
    ThrowOnConstruct() { throw std::runtime_error("Constructor failed"); }
};

struct ThrowOnCopy {
    static int alive;

    ThrowOnCopy() { alive++; }
    ThrowOnCopy(const ThrowOnCopy&) { throw std::runtime_error("Copy failed"); }
    ~ThrowOnCopy() { alive--; }
};

int ThrowOnCopy::alive = 0;

struct MoveOnly {
    int value;

    explicit MoveOnly(int value) : value(value) {}
    MoveOnly(const MoveOnly&) = delete;
};

// Полиморфная иерархия: владеть Dog через UnqPtr<Animal> можно
struct Animal {
    static int destroyed;

    virtual ~Animal() { destroyed++; }
    virtual int id() const { return 0; }
};

int Animal::destroyed = 0;

struct Dog final : Animal {
    static int destroyed;

    ~Dog() override { destroyed++; }
    int id() const override { return 1; }
};

int Dog::destroyed = 0;

// Множественное наследование без виртуального деструктора: адрес SecondBase смещён
struct FirstBase {
    int first = 10;
};

struct SecondBase {
    int second = 20;
};

struct Child : FirstBase, SecondBase {
    static int destroyed;

    ~Child() { destroyed++; }
};

int Child::destroyed = 0;

struct UniqueNode {
    Tracked value;
    UnqPtr<UniqueNode> next;

    explicit UniqueNode(int value) : value(value) {}
};

struct SharedNode {
    Tracked value;
    ShrdPtr<SharedNode> next;

    explicit SharedNode(int value) : value(value) {}
};

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

// Второе присваивание бросает: так клон массива падает посередине
struct ThrowArrayElement {
    static int alive;
    static int assignments;

    ThrowArrayElement() { alive++; }
    ThrowArrayElement& operator=(const ThrowArrayElement&) {
        if (++assignments == 2) throw std::runtime_error("Array copy failed");
        return *this;
    }
    ~ThrowArrayElement() { alive--; }
};

int ThrowArrayElement::alive = 0;
int ThrowArrayElement::assignments = 0;

struct MoveOnlyElement {
    int value = 0;

    MoveOnlyElement() = default;
    MoveOnlyElement& operator=(const MoveOnlyElement&) = delete;
};

// Есть ли у указателя доступный clone(): нужен, чтобы static_assert мог проверить его отсутствие
template <class Pointer, class = void>
struct has_clone : std::false_type {};

template <class Pointer>
struct has_clone<Pointer, std::void_t<decltype(std::declval<const Pointer&>().clone())>>
    : std::true_type {};

// Число выделений кучи с момента снимка
size_t allocations_since(const AllocSnapshot& before) {
    return alloc_snapshot().allocations - before.allocations;
}

// ---------------------------------------------------------------- проверки при компиляции

static_assert(sizeof(UnqPtr<int>) == sizeof(int*), "UnqPtr must be as small as a raw pointer");
static_assert(sizeof(ShrdPtr<int>) == 2 * sizeof(void*), "ShrdPtr is a block and an address");
static_assert(std::is_nothrow_move_constructible<UnqPtr<int>>::value, "Move must not throw");
static_assert(std::is_nothrow_move_assignable<UnqPtr<int>>::value, "Move must not throw");
static_assert(std::is_nothrow_move_constructible<ShrdPtr<int>>::value, "Move must not throw");
static_assert(std::is_nothrow_copy_assignable<ShrdPtr<int>>::value, "Copy must not throw");

// UnqPtr: только перемещение, владение сырым адресом не принимается
static_assert(!std::is_copy_constructible<UnqPtr<int>>::value, "UnqPtr must be move-only");
static_assert(!std::is_copy_assignable<UnqPtr<int>>::value, "UnqPtr must be move-only");
static_assert(!std::is_constructible<UnqPtr<int>, int*>::value, "No ownership of raw pointers");
static_assert(!std::is_constructible<UnqPtr<int>, UnqPtr<int>&>::value, "Lvalue must be moved");

// Подтипизация UnqPtr: Derived -> Base только при виртуальном деструкторе Base
static_assert(std::is_convertible<UnqPtr<Dog>, UnqPtr<Animal>>::value, "Upcast must work");
static_assert(!std::is_constructible<UnqPtr<Dog>, UnqPtr<Animal>&&>::value, "No downcast");
static_assert(!std::is_constructible<UnqPtr<SecondBase>, UnqPtr<Child>&&>::value,
              "delete SecondBase* would not destroy Child");
static_assert(std::is_convertible<UnqPtr<int>, UnqPtr<const int>>::value, "Adding const is ok");
static_assert(!std::is_constructible<UnqPtr<int>, UnqPtr<const int>&&>::value, "No const drop");
static_assert(!std::is_constructible<UnqPtr<long>, UnqPtr<int>&&>::value, "Unrelated types");
static_assert(!std::is_constructible<UnqPtr<SecondBase[]>, UnqPtr<Child[]>&&>::value,
              "Arrays must not be covariant");

// clone(): нужен копирующий конструктор и отсутствие срезки
static_assert(has_clone<UnqPtr<int>>::value, "Copyable type can be cloned");
static_assert(!has_clone<UnqPtr<MoveOnly>>::value, "Non-copyable type cannot be cloned");
static_assert(!has_clone<UnqPtr<Animal>>::value, "Animal* may hide Dog: clone would slice");
static_assert(has_clone<UnqPtr<Dog>>::value, "Final type cannot hide a derived object");
static_assert(has_clone<UnqPtr<int[]>>::value, "Copy-assignable elements can be cloned");
static_assert(!has_clone<UnqPtr<MoveOnlyElement[]>>::value, "Elements cannot be copied");

// ShrdPtr: блок помнит настоящий тип, поэтому виртуальный деструктор не нужен
static_assert(std::is_convertible<ShrdPtr<Child>, ShrdPtr<SecondBase>>::value, "Upcast copy");
static_assert(!std::is_constructible<ShrdPtr<Child>, ShrdPtr<SecondBase>>::value, "No downcast");
static_assert(!std::is_constructible<ShrdPtr<long>, ShrdPtr<int>>::value, "Unrelated types");
static_assert(std::is_constructible<ShrdPtr<SecondBase>, UnqPtr<Child>&&>::value,
              "Transfer remembers Child deleter");
static_assert(!std::is_constructible<ShrdPtr<int>, UnqPtr<int>&>::value, "Lvalue must be moved");
static_assert(!std::is_constructible<UnqPtr<int>, ShrdPtr<int>&&>::value,
              "Shared ownership cannot become unique");
static_assert(!std::is_constructible<ShrdPtr<int>, int*>::value, "No ownership of raw pointers");

// WeakPtr
static_assert(sizeof(WeakPtr<int>) == 2 * sizeof(void*), "WeakPtr is a block and an address");
static_assert(std::is_nothrow_move_constructible<WeakPtr<int>>::value, "Move must not throw");
static_assert(std::is_convertible<ShrdPtr<Child>, WeakPtr<SecondBase>>::value, "Upcast from owner");
static_assert(!std::is_constructible<WeakPtr<Child>, ShrdPtr<SecondBase>>::value, "No downcast");
static_assert(!std::is_constructible<WeakPtr<int>, int*>::value, "No raw pointers");
static_assert(!std::is_constructible<ShrdPtr<int>, WeakPtr<int>>::value, "Ownership only via lock()");

// ---------------------------------------------------------------- UnqPtr: состояния

// Пустой указатель отвечает на вопросы, а разыменование бросает исключение.
TEST(UnqPtrTest, EmptyPointerState) {
    UnqPtr<Tracked> empty;
    EXPECT_FALSE(empty);
    EXPECT_EQ(empty.get(), nullptr);
    EXPECT_THROW(*empty, std::logic_error);
    EXPECT_THROW(empty->value, std::logic_error);
    EXPECT_FALSE(empty.clone());
}

// reset() и swap() пустых указателей ничего не ломают.
TEST(UnqPtrTest, EmptyPointerResetAndSwap) {
    UnqPtr<Tracked> empty;
    empty.reset();
    UnqPtr<Tracked> other;
    empty.swap(other);
    EXPECT_FALSE(empty);
    EXPECT_FALSE(other);
}

// make() создаёт объект, reset() удаляет его, повторный reset() ничего не делает.
TEST(UnqPtrTest, MakeOwnsAndResetDestroys) {
    auto owner = UnqPtr<Tracked>::make(5);
    EXPECT_EQ(owner->value, 5);
    EXPECT_EQ(Tracked::alive, 1);

    owner.reset();
    EXPECT_FALSE(owner);
    EXPECT_EQ(Tracked::alive, 0);
    owner.reset();
    EXPECT_EQ(Tracked::alive, 0);
}

// Источник после перемещения пуст, но им можно пользоваться дальше.
TEST(UnqPtrTest, MovedFromPointerIsEmptyAndReusable) {
    auto first = UnqPtr<Tracked>::make(1);
    Tracked* address = first.get();
    UnqPtr<Tracked> second = std::move(first);

    EXPECT_FALSE(first);
    EXPECT_EQ(second.get(), address);
    first = UnqPtr<Tracked>::make(2);
    EXPECT_EQ(first->value, 2);
    EXPECT_EQ(Tracked::alive, 2);
}

// owner = std::move(owner): объект не удаляется.
TEST(UnqPtrTest, SelfMoveAssignmentKeepsObject) {
    auto owner = UnqPtr<Tracked>::make(7);
    UnqPtr<Tracked>& alias = owner; // Через ссылку, чтобы компилятор не предупреждал о самоприсваивании
    owner = std::move(alias);

    ASSERT_TRUE(owner);
    EXPECT_EQ(owner->value, 7);
    EXPECT_EQ(Tracked::alive, 1);
}

// Присваивание удаляет прежний объект назначения ровно один раз.
TEST(UnqPtrTest, MoveAssignmentDestroysPreviousObject) {
    auto target = UnqPtr<Tracked>::make(1);
    auto source = UnqPtr<Tracked>::make(2);

    target = std::move(source);
    EXPECT_EQ(target->value, 2);
    EXPECT_FALSE(source);
    EXPECT_EQ(Tracked::alive, 1);
}

// head = std::move(head->next): источник живёт внутри удаляемого объекта.
// Если сначала удалить старый объект, а потом читать источник, это use-after-free.
TEST(UnqPtrTest, AssignmentFromObjectOwnedByTarget) {
    auto head = UnqPtr<UniqueNode>::make(1);
    head->next = UnqPtr<UniqueNode>::make(2);
    head->next->next = UnqPtr<UniqueNode>::make(3);
    EXPECT_EQ(Tracked::alive, 3);

    head = std::move(head->next);
    EXPECT_EQ(head->value.value, 2);
    EXPECT_EQ(Tracked::alive, 2);

    head = std::move(head->next);
    EXPECT_EQ(head->value.value, 3);
    EXPECT_FALSE(head->next);
    EXPECT_EQ(Tracked::alive, 1);
}

// Деструктор объекта смотрит на своего владельца: тот уже должен быть пуст,
// иначе удаление из деструктора удалило бы объект второй раз.
TEST(UnqPtrTest, ResetIsReentrant) {
    struct Self {
        UnqPtr<Self>* owner = nullptr;
        bool* seen_empty = nullptr;
        ~Self() { *seen_empty = !*owner; }
    };
    bool owner_was_empty = false;
    auto self = UnqPtr<Self>::make();
    self->owner = &self;
    self->seen_empty = &owner_was_empty;
    self.reset();
    EXPECT_TRUE(owner_was_empty);
}

TEST(UnqPtrTest, SwapExchangesObjects) {
    auto first = UnqPtr<Tracked>::make(1);
    auto second = UnqPtr<Tracked>::make(2);
    first.swap(second);
    EXPECT_EQ(first->value, 2);
    EXPECT_EQ(second->value, 1);
}

TEST(UnqPtrTest, SwapWithEmpty) {
    auto first = UnqPtr<Tracked>::make(2);
    UnqPtr<Tracked> empty;
    first.swap(empty);
    EXPECT_FALSE(first);
    EXPECT_EQ(empty->value, 2);
    EXPECT_EQ(Tracked::alive, 1);
}

// ---------------------------------------------------------------- UnqPtr: исключения

TEST(UnqPtrTest, ThrowingConstructorLeaksNothing) {
    EXPECT_THROW(UnqPtr<ThrowOnConstruct>::make(), std::runtime_error);
}

// AllocFailureScope(1): первое же выделение памяти бросает std::bad_alloc.
TEST(UnqPtrTest, FailedAllocationLeaksNothing) {
    AllocFailureScope failure(1);
    EXPECT_THROW(UnqPtr<Tracked>::make(1), std::bad_alloc);
    EXPECT_EQ(Tracked::alive, 0);
}

// clone() создаёт отдельный объект: изменение и удаление копии не затрагивают оригинал.
TEST(UnqPtrTest, CloneIsIndependent) {
    auto owner = UnqPtr<Tracked>::make(10);
    auto copy = owner.clone();

    EXPECT_NE(owner.get(), copy.get());
    copy->value = 20;
    EXPECT_EQ(owner->value, 10);
    owner.reset();
    EXPECT_EQ(Tracked::alive, 1);
    EXPECT_EQ(copy->value, 20);
}

// Исключение копирующего конструктора при clone() не трогает оригинал.
TEST(UnqPtrTest, FailedCloneKeepsOriginal) {
    auto owner = UnqPtr<ThrowOnCopy>::make();
    EXPECT_THROW(owner.clone(), std::runtime_error);
    EXPECT_TRUE(owner);
    EXPECT_EQ(ThrowOnCopy::alive, 1);
    owner.reset();
    EXPECT_EQ(ThrowOnCopy::alive, 0);
}

TEST(UnqPtrTest, ConstObjectCanBeMadeAndCloned) {
    auto owner = UnqPtr<const Tracked>::make(3);
    auto copy = owner.clone();
    EXPECT_EQ(copy->value, 3);
    EXPECT_EQ(Tracked::alive, 2);
}

TEST(UnqPtrTest, TransferAddsConst) {
    UnqPtr<const Tracked> from_mutable = UnqPtr<Tracked>::make(4);
    EXPECT_EQ(from_mutable->value, 4);
    EXPECT_EQ(Tracked::alive, 1);
}

// Некопируемым объектом можно владеть и передавать его.
TEST(UnqPtrTest, MoveOnlyObjectCanBeOwned) {
    auto owner = UnqPtr<MoveOnly>::make(13);
    UnqPtr<MoveOnly> moved = std::move(owner);
    EXPECT_EQ(moved->value, 13);
}

// ---------------------------------------------------------------- UnqPtr: наследование

// UnqPtr<Animal> вызывает виртуальный метод и деструктор Dog.
TEST(UnqPtrTest, UpcastDeletesDerivedObject) {
    Animal::destroyed = 0;
    Dog::destroyed = 0;
    UnqPtr<Animal> animal = UnqPtr<Dog>::make();
    EXPECT_EQ(animal->id(), 1);

    animal = UnqPtr<Animal>::make();
    EXPECT_EQ(Dog::destroyed, 1);
    EXPECT_EQ(animal->id(), 0);
    animal.reset();
    EXPECT_EQ(Animal::destroyed, 2); // Dog был и Animal тоже
}

// ---------------------------------------------------------------- UnqPtr<T[]>

// Клон массива независим, каждый массив удаляется отдельно.
TEST(UnqPtrArrayTest, CloneIsIndependent) {
    auto owner = UnqPtr<ArrayElement[]>::make_array(3);
    owner[1].value = 42;

    auto copy = owner.clone();
    EXPECT_EQ(ArrayElement::alive, 6);
    copy[1].value = 7;
    EXPECT_EQ(owner[1].value, 42);

    owner.reset();
    EXPECT_EQ(ArrayElement::alive, 3);
    copy.reset();
    EXPECT_EQ(ArrayElement::alive, 0);
}

// Если присваивание элемента бросает, частичная копия удаляется, оригинал остаётся.
TEST(UnqPtrArrayTest, FailedCloneDestroysPartialCopy) {
    ThrowArrayElement::assignments = 0;
    auto owner = UnqPtr<ThrowArrayElement[]>::make_array(3);
    EXPECT_THROW(owner.clone(), std::runtime_error);
    EXPECT_EQ(ThrowArrayElement::alive, 3);
    EXPECT_TRUE(owner);
    owner.reset();
    EXPECT_EQ(ThrowArrayElement::alive, 0);
}

// Граничные индексы: последний допустим, size() и SIZE_MAX — нет.
TEST(UnqPtrArrayTest, IndexBoundaries) {
    auto owner = UnqPtr<int[]>::make_array(3);
    EXPECT_EQ(owner[0], 0);
    EXPECT_EQ(owner[2], 0);
    EXPECT_THROW(owner[3], std::out_of_range);
    EXPECT_THROW(owner[SIZE_MAX], std::out_of_range);
}

// Массив нулевой длины не выделяет память и ведёт себя как пустой указатель.
TEST(UnqPtrArrayTest, ZeroLengthArrayIsEmpty) {
    auto before = alloc_snapshot();
    auto owner = UnqPtr<int[]>::make_array(0);
    EXPECT_EQ(allocations_since(before), 0u);
    EXPECT_FALSE(owner);
    EXPECT_EQ(owner.size(), 0u);
    EXPECT_THROW(owner[0], std::out_of_range);
    EXPECT_FALSE(owner.clone());
}

// Невыполнимый размер бросает исключение без утечек и частично созданных элементов.
TEST(UnqPtrArrayTest, ImpossibleSizeThrows) {
    // volatile: иначе при -O2 компилятор видит константу и предупреждает о гигантском memset
    volatile size_t impossible = SIZE_MAX / sizeof(ArrayElement);
    EXPECT_THROW(UnqPtr<ArrayElement[]>::make_array(impossible), std::bad_alloc);
    EXPECT_EQ(ArrayElement::alive, 0);
}

TEST(UnqPtrArrayTest, MoveTransfersDataAndLength) {
    auto first = UnqPtr<int[]>::make_array(4);
    int* address = first.get();
    UnqPtr<int[]> second = std::move(first);
    EXPECT_EQ(first.size(), 0u);
    EXPECT_EQ(second.get(), address);
    EXPECT_EQ(second.size(), 4u);
}

TEST(UnqPtrArrayTest, SelfMoveKeepsArray) {
    auto owner = UnqPtr<int[]>::make_array(4);
    int* address = owner.get();
    UnqPtr<int[]>& alias = owner;
    owner = std::move(alias);
    EXPECT_EQ(owner.get(), address);
    EXPECT_EQ(owner.size(), 4u);
}

// ---------------------------------------------------------------- ShrdPtr: состояния

TEST(ShrdPtrTest, EmptyPointerState) {
    ShrdPtr<Tracked> empty;
    EXPECT_FALSE(empty);
    EXPECT_EQ(empty.use_count(), 0u);
    EXPECT_THROW(*empty, std::logic_error);
    EXPECT_THROW(empty->value, std::logic_error);
    empty.reset();
}

// У пустого указателя нет блока: его копия тоже пуста и памяти не просит.
TEST(ShrdPtrTest, CopyOfEmptyAllocatesNothing) {
    ShrdPtr<Tracked> empty;
    auto before = alloc_snapshot();
    ShrdPtr<Tracked> copy = empty;
    EXPECT_EQ(allocations_since(before), 0u);
    EXPECT_EQ(copy.use_count(), 0u);
}

// make() кладёт объект прямо в блок со счётчиками: одно выделение на всё.
TEST(ShrdPtrTest, MakeAllocatesOnce) {
    auto before = alloc_snapshot();
    auto owner = ShrdPtr<Tracked>::make(1);
    EXPECT_EQ(allocations_since(before), 1u);
}

// Копия только увеличивает счётчик в уже готовом блоке.
TEST(ShrdPtrTest, CopiesAllocateNothing) {
    auto first = ShrdPtr<Tracked>::make(1);
    auto before = alloc_snapshot();
    auto second = first;
    ShrdPtr<Tracked> third(second);
    EXPECT_EQ(allocations_since(before), 0u);
    EXPECT_EQ(first.use_count(), 3u);
}

// Блок освобождает последний владелец, а не первый.
TEST(ShrdPtrTest, LastOwnerFreesBlock) {
    auto first = ShrdPtr<Tracked>::make(1);
    auto second = first;
    auto live = alloc_snapshot().live();
    first.reset();
    EXPECT_EQ(alloc_snapshot().live(), live);
    second.reset();
    EXPECT_EQ(alloc_snapshot().live(), live - 1);
    EXPECT_EQ(Tracked::alive, 0);
}

// Копии держат один объект, удаляет его последний владелец.
TEST(ShrdPtrTest, CopiesShareOneObject) {
    auto first = ShrdPtr<Tracked>::make(1);
    auto second = first;
    second->value = 7;
    EXPECT_EQ(first->value, 7);

    first.reset();
    EXPECT_EQ(Tracked::alive, 1);
    EXPECT_EQ(second.use_count(), 1u);
    second.reset();
    EXPECT_EQ(Tracked::alive, 0);
}

TEST(ShrdPtrTest, ThrowingConstructorLeaksNothing) {
    EXPECT_THROW(ShrdPtr<ThrowOnConstruct>::make(), std::runtime_error);
}

TEST(ShrdPtrTest, FailedAllocationLeaksNothing) {
    AllocFailureScope failure(1);
    EXPECT_THROW(ShrdPtr<Tracked>::make(1), std::bad_alloc);
    EXPECT_EQ(Tracked::alive, 0);
}

// owner = owner не меняет счётчик и не удаляет объект.
TEST(ShrdPtrTest, SelfCopyAssignmentKeepsObject) {
    auto owner = ShrdPtr<Tracked>::make(3);
    ShrdPtr<Tracked>& alias = owner;
    owner = alias;
    ASSERT_TRUE(owner);
    EXPECT_EQ(owner.use_count(), 1u);
    EXPECT_EQ(owner->value, 3);
}

// owner = std::move(owner) тоже.
TEST(ShrdPtrTest, SelfMoveAssignmentKeepsObject) {
    auto owner = ShrdPtr<Tracked>::make(3);
    ShrdPtr<Tracked>& alias = owner;
    owner = std::move(alias);
    ASSERT_TRUE(owner);
    EXPECT_EQ(owner.use_count(), 1u);
    EXPECT_EQ(owner->value, 3);
}

// Перемещение не меняет счётчик и опустошает источник.
TEST(ShrdPtrTest, MoveDoesNotChangeUseCount) {
    auto first = ShrdPtr<int>::make(11);
    auto second = first;
    ShrdPtr<int> moved = std::move(first);

    EXPECT_FALSE(first);
    EXPECT_EQ(first.use_count(), 0u);
    EXPECT_EQ(moved.use_count(), 2u);
    moved.reset();
    EXPECT_EQ(second.use_count(), 1u);
}

// Замена объекта в одном владельце не удаляет его, пока жив другой владелец.
TEST(ShrdPtrTest, AssignmentKeepsOldObjectForOtherOwners) {
    auto first = ShrdPtr<Tracked>::make(1);
    auto old = first;

    first = ShrdPtr<Tracked>::make(2);
    EXPECT_EQ(old->value, 1);
    EXPECT_EQ(old.use_count(), 1u);
    EXPECT_EQ(Tracked::alive, 2);
    old.reset();
    EXPECT_EQ(Tracked::alive, 1);
}

// Присваивание отпускает прежний объект, если назначение было последним владельцем.
TEST(ShrdPtrTest, AssignmentReleasesPreviousObject) {
    auto first = ShrdPtr<Tracked>::make(1);
    auto second = ShrdPtr<Tracked>::make(2);

    first = second;
    EXPECT_EQ(first->value, 2);
    EXPECT_EQ(second.use_count(), 2u);
    EXPECT_EQ(Tracked::alive, 1);
}

// head = head->next, где head — единственный владелец первого узла:
// источник надо скопировать раньше, чем удалится узел, внутри которого он лежит.
TEST(ShrdPtrTest, CopyAssignmentFromObjectOwnedByTarget) {
    auto head = ShrdPtr<SharedNode>::make(1);
    head->next = ShrdPtr<SharedNode>::make(2);

    head = head->next;
    EXPECT_EQ(head->value.value, 2);
    EXPECT_EQ(head.use_count(), 1u);
    EXPECT_EQ(Tracked::alive, 1);
}

// То же для перемещения: head = std::move(head->next).
TEST(ShrdPtrTest, MoveAssignmentFromObjectOwnedByTarget) {
    auto head = ShrdPtr<SharedNode>::make(2);
    head->next = ShrdPtr<SharedNode>::make(3);

    head = std::move(head->next);
    EXPECT_EQ(head->value.value, 3);
    EXPECT_EQ(Tracked::alive, 1);
}

// Во время удаления объекта его владелец уже пуст.
TEST(ShrdPtrTest, ResetIsReentrant) {
    struct Self {
        ShrdPtr<Self>* owner = nullptr;
        bool* seen_empty = nullptr;
        ~Self() { *seen_empty = !*owner && owner->use_count() == 0; }
    };
    bool owner_was_empty = false;
    auto self = ShrdPtr<Self>::make();
    self->owner = &self;
    self->seen_empty = &owner_was_empty;
    self.reset();
    EXPECT_TRUE(owner_was_empty);
}

// Равенство - один и тот же объект, в том числе через указатель на базу.
TEST(ShrdPtrTest, EqualityComparesObjects) {
    auto dog = ShrdPtr<Dog>::make();
    ShrdPtr<Animal> animal = dog;
    auto other = ShrdPtr<Dog>::make();
    EXPECT_TRUE(dog == animal);
    EXPECT_FALSE(dog != animal);
    EXPECT_TRUE(dog != other);
    EXPECT_TRUE(ShrdPtr<int>() == ShrdPtr<int>());
    EXPECT_TRUE(dog != ShrdPtr<Animal>());
}

// Приведение к наследнику делит владение: тот же объект, счётчик +1, без выделений.
TEST(ShrdPtrTest, StaticPointerCastSharesOwnership) {
    Dog::destroyed = 0;
    ShrdPtr<Animal> animal = ShrdPtr<Dog>::make();
    auto before = alloc_snapshot();
    ShrdPtr<Dog> dog = static_pointer_cast<Dog>(animal);
    EXPECT_EQ(allocations_since(before), 0u);
    EXPECT_TRUE(dog == animal);
    EXPECT_EQ(animal.use_count(), 2u);
    EXPECT_EQ(dog->id(), 1);

    animal.reset();
    EXPECT_EQ(Dog::destroyed, 0);
    dog.reset();
    EXPECT_EQ(Dog::destroyed, 1);
}

TEST(ShrdPtrTest, StaticPointerCastOfEmptyIsEmpty) {
    EXPECT_FALSE(static_pointer_cast<Dog>(ShrdPtr<Animal>()));
}

// ---------------------------------------------------------------- ShrdPtr: передача из UnqPtr

// Передача сохраняет адрес, опустошает UnqPtr и выделяет только блок.
TEST(ShrdPtrTest, TakesOwnershipFromUnqPtr) {
    auto unique = UnqPtr<Tracked>::make(17);
    Tracked* address = unique.get();

    auto before = alloc_snapshot();
    ShrdPtr<Tracked> shared(std::move(unique));
    EXPECT_EQ(allocations_since(before), 1u);
    EXPECT_FALSE(unique);
    EXPECT_EQ(shared.get(), address);
    EXPECT_EQ(shared.use_count(), 1u);

    auto second = shared;
    shared.reset();
    EXPECT_EQ(Tracked::alive, 1);
    second.reset();
    EXPECT_EQ(Tracked::alive, 0);
}

// Из пустого UnqPtr получается пустой ShrdPtr без выделения блока.
TEST(ShrdPtrTest, TransferFromEmptyUnqPtr) {
    UnqPtr<Tracked> unique;
    auto before = alloc_snapshot();
    ShrdPtr<Tracked> shared(std::move(unique));
    EXPECT_EQ(allocations_since(before), 0u);
    EXPECT_FALSE(shared);
    EXPECT_EQ(shared.use_count(), 0u);
}

// Если блок не выделился, объект остаётся у UnqPtr (строгая гарантия).
TEST(ShrdPtrTest, FailedTransferKeepsUnqPtrOwner) {
    auto unique = UnqPtr<Tracked>::make(5);
    Tracked* address = unique.get();
    {
        AllocFailureScope failure(1);
        EXPECT_THROW(ShrdPtr<Tracked> shared(std::move(unique)), std::bad_alloc);
    }
    EXPECT_EQ(unique.get(), address);
    EXPECT_EQ(unique->value, 5);
    EXPECT_EQ(Tracked::alive, 1);
}

// ---------------------------------------------------------------- ShrdPtr: наследование

// Child -> SecondBase: адрес смещается, а удаляется Child, хотя деструктор не виртуальный.
TEST(ShrdPtrTest, TransferDerivedWithoutVirtualDestructor) {
    Child::destroyed = 0;
    auto child = UnqPtr<Child>::make();
    SecondBase* expected = child.get();
    EXPECT_NE(static_cast<void*>(expected), static_cast<void*>(child.get()));

    ShrdPtr<SecondBase> shared(std::move(child));
    EXPECT_EQ(shared.get(), expected);
    EXPECT_EQ(shared->second, 20);
    shared.reset();
    EXPECT_EQ(Child::destroyed, 1);
}

// Копии с преобразованием типа делят один счётчик.
TEST(ShrdPtrTest, UpcastSharesOwnership) {
    Child::destroyed = 0;
    auto child = ShrdPtr<Child>::make();
    SecondBase* expected = child.get();

    ShrdPtr<SecondBase> base(child);
    ShrdPtr<SecondBase> second = child;
    EXPECT_EQ(base.get(), expected);
    EXPECT_EQ(second.get(), expected);
    EXPECT_EQ(base.use_count(), 3u);

    child.reset();
    base.reset();
    EXPECT_EQ(Child::destroyed, 0);
    second.reset();
    EXPECT_EQ(Child::destroyed, 1);
}

// Присваивание с преобразованием типа отпускает прежний объект.
TEST(ShrdPtrTest, ConvertingAssignment) {
    Child::destroyed = 0;
    auto child = ShrdPtr<Child>::make();
    ShrdPtr<SecondBase> base = ShrdPtr<Child>::make();

    base = child;
    EXPECT_EQ(Child::destroyed, 1);
    EXPECT_EQ(child.use_count(), 2u);
    base = ShrdPtr<Child>::make();
    EXPECT_EQ(child.use_count(), 1u);
    EXPECT_EQ(base.use_count(), 1u);
}

// ---------------------------------------------------------------- ShrdPtr: циклы

struct CycleNode {
    static int alive;
    ShrdPtr<CycleNode> next;

    CycleNode() { alive++; }
    ~CycleNode() { alive--; }
};

int CycleNode::alive = 0;

// Ссылка на себя удерживает объект после ухода внешнего владельца, пока цикл не разорван.
TEST(ShrdPtrTest, SelfReferenceKeepsObjectAliveUntilBroken) {
    auto owner = ShrdPtr<CycleNode>::make();
    owner->next = owner;
    EXPECT_EQ(owner.use_count(), 2u);

    CycleNode* address = owner.get();
    owner.reset();
    ASSERT_EQ(CycleNode::alive, 1);
    EXPECT_EQ(address->next.use_count(), 1u);

    // Разрыв через reset() самого поля: объект удаляется изнутри своего же члена
    address->next.reset();
    EXPECT_EQ(CycleNode::alive, 0);
}

// Два объекта со ссылками друг на друга живут без внешних владельцев до разрыва цикла.
TEST(ShrdPtrTest, OwnershipCycleKeepsObjectsAliveUntilBroken) {
    auto first = ShrdPtr<CycleNode>::make();
    auto second = ShrdPtr<CycleNode>::make();
    first->next = second;
    second->next = first;

    CycleNode* address = first.get();
    first.reset();
    second.reset();
    ASSERT_EQ(CycleNode::alive, 2);

    address->next->next.reset(); // second отпускает first
    EXPECT_EQ(CycleNode::alive, 0);
}

// ---------------------------------------------------------------- ShrdPtr: потоки

constexpr int THREADS = 8;

// Потоки одновременно копируют и отпускают один объект: счётчик не теряет изменений.
// С неатомарным ++/-- итоговый use_count «плывёт», а объект удаляется раньше времени.
TEST(ShrdPtrThreadTest, ConcurrentCopiesKeepCountConsistent) {
    auto shared = ShrdPtr<Tracked>::make(1);
    std::vector<std::thread> workers;
    for (int thread = 0; thread < THREADS; thread++) {
        workers.emplace_back([&shared] {
            for (int step = 0; step < 100000; step++) {
                ShrdPtr<Tracked> copy = shared;
                ShrdPtr<Tracked> second = copy;
                second.reset();
            }
        });
    }
    for (auto& worker : workers) worker.join();

    EXPECT_EQ(shared.use_count(), 1u);
    EXPECT_EQ(Tracked::alive, 1);
    shared.reset();
    EXPECT_EQ(Tracked::alive, 0);
}

// Каждый поток пишет в свою ячейку и отпускает копию; объект удаляет последний из них.
// acquire-release на счётчике гарантирует, что деструктор видит записи всех потоков.
struct Slots {
    static int destroyed;
    static long long seen_sum;
    long long values[THREADS] = {};

    ~Slots() {
        destroyed++;
        seen_sum = 0;
        for (long long value : values) seen_sum += value;
    }
};

int Slots::destroyed = 0;
long long Slots::seen_sum = 0;

TEST(ShrdPtrThreadTest, LastOwnerInAnyThreadSeesAllWrites) {
    Slots::destroyed = 0;
    auto shared = ShrdPtr<Slots>::make();
    std::vector<std::thread> workers;
    for (int thread = 0; thread < THREADS; thread++) {
        workers.emplace_back([copy = shared, thread]() mutable {
            for (int step = 1; step <= 1000; step++) copy->values[thread] += step;
            copy.reset();
        });
    }
    shared.reset(); // теперь последним владельцем окажется один из потоков
    for (auto& worker : workers) worker.join();

    EXPECT_EQ(Slots::destroyed, 1);
    EXPECT_EQ(Slots::seen_sum, THREADS * 500500LL); // 1 + 2 + ... + 1000 = 500500 от каждого потока
}

// ---------------------------------------------------------------- ShrdPtr<T[]>

// Общий массив живёт до последнего владельца.
TEST(ShrdPtrArrayTest, LastOwnerDeletesArray) {
    auto first = ShrdPtr<ArrayElement[]>::make_array(3);
    first[1].value = 42;
    auto second = first;

    EXPECT_EQ(first.use_count(), 2u);
    first.reset();
    EXPECT_EQ(ArrayElement::alive, 3);
    EXPECT_EQ(second.size(), 3u);
    EXPECT_EQ(second[1].value, 42);
    second.reset();
    EXPECT_EQ(ArrayElement::alive, 0);
}

TEST(ShrdPtrArrayTest, IndexBoundaries) {
    auto shared = ShrdPtr<ArrayElement[]>::make_array(3);
    EXPECT_NO_THROW(shared[2]);
    EXPECT_THROW(shared[3], std::out_of_range);
}

// Передача массива из UnqPtr сохраняет адрес и длину без копирования элементов.
TEST(ShrdPtrArrayTest, TakesOwnershipFromUnqPtr) {
    auto unique = UnqPtr<ArrayElement[]>::make_array(3);
    auto* address = unique.get();
    unique[1].value = 42;

    ShrdPtr<ArrayElement[]> shared(std::move(unique));
    EXPECT_FALSE(unique);
    EXPECT_EQ(unique.size(), 0u);
    EXPECT_EQ(shared.get(), address);
    EXPECT_EQ(shared.size(), 3u);
    EXPECT_EQ(shared[1].value, 42);
    EXPECT_EQ(ArrayElement::alive, 3);
}

// Если блок не выделился, массив остаётся у UnqPtr.
TEST(ShrdPtrArrayTest, FailedTransferKeepsArray) {
    auto unique = UnqPtr<ArrayElement[]>::make_array(2);
    {
        AllocFailureScope failure(1);
        EXPECT_THROW(ShrdPtr<ArrayElement[]> shared(std::move(unique)), std::bad_alloc);
    }
    EXPECT_EQ(unique.size(), 2u);
    EXPECT_EQ(ArrayElement::alive, 2);
}

// Массив нулевой длины пуст и не выделяет ни массив, ни блок.
TEST(ShrdPtrArrayTest, ZeroLengthArrayIsEmpty) {
    auto before = alloc_snapshot();
    auto shared = ShrdPtr<int[]>::make_array(0);
    EXPECT_EQ(allocations_since(before), 0u);
    EXPECT_FALSE(shared);
    EXPECT_EQ(shared.size(), 0u);
    EXPECT_EQ(shared.use_count(), 0u);
    EXPECT_THROW(shared[0], std::out_of_range);
}

// ---------------------------------------------------------------- WeakPtr

// Пустая ссылка: объекта нет, lock() даёт пустой ShrdPtr.
TEST(WeakPtrTest, EmptyReferenceState) {
    WeakPtr<Tracked> weak;
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(weak.use_count(), 0u);
    EXPECT_FALSE(weak.lock());
    weak.reset();
    EXPECT_TRUE(weak.expired());
}

// Слабая ссылка не владеет: счётчик владельцев не растёт, память не выделяется.
TEST(WeakPtrTest, ObservesWithoutOwning) {
    auto shared = ShrdPtr<Tracked>::make(5);
    auto before = alloc_snapshot();
    WeakPtr<Tracked> weak = shared;
    EXPECT_EQ(allocations_since(before), 0u);
    EXPECT_EQ(weak.use_count(), 1u);

    ShrdPtr<Tracked> locked = weak.lock();
    EXPECT_EQ(locked.get(), shared.get());
    EXPECT_EQ(shared.use_count(), 2u);
}

// Пока жив хоть один ShrdPtr, lock() его находит; после последнего - пусто.
TEST(WeakPtrTest, ExpiresWithLastOwner) {
    auto shared = ShrdPtr<Tracked>::make(5);
    WeakPtr<Tracked> weak = shared;
    ShrdPtr<Tracked> locked = weak.lock();

    shared.reset();
    EXPECT_FALSE(weak.expired());
    EXPECT_EQ(weak.lock()->value, 5);

    locked.reset();
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(Tracked::alive, 0);
    EXPECT_FALSE(weak.lock());
}

// Объект из make() лежит в блоке, но умирает раньше блока: блок держат слабые ссылки.
TEST(WeakPtrTest, BlockOutlivesObject) {
    auto live = alloc_snapshot().live();
    auto shared = ShrdPtr<Tracked>::make(1);
    WeakPtr<Tracked> weak = shared;
    WeakPtr<Tracked> second = weak;

    shared.reset();
    EXPECT_EQ(Tracked::alive, 0);
    EXPECT_EQ(alloc_snapshot().live(), live + 1);
    weak.reset();
    EXPECT_EQ(alloc_snapshot().live(), live + 1);
    second.reset();
    EXPECT_EQ(alloc_snapshot().live(), live);
}

// Блок из UnqPtr удаляет объект так, как он был создан.
TEST(WeakPtrTest, WorksWithTransferredObject) {
    Dog::destroyed = 0;
    ShrdPtr<Animal> animal(UnqPtr<Dog>::make());
    WeakPtr<Animal> weak = animal;
    animal.reset();
    EXPECT_EQ(Dog::destroyed, 1);
    EXPECT_TRUE(weak.expired());
}

// WeakPtr<Animal> из временного ShrdPtr<Dog>: владельцев нет, Dog удалён сразу.
TEST(WeakPtrTest, WorksWithUpcast) {
    Dog::destroyed = 0;
    WeakPtr<Animal> from_dog = ShrdPtr<Dog>::make();
    EXPECT_EQ(Dog::destroyed, 1);
    EXPECT_TRUE(from_dog.expired());
}

TEST(WeakPtrTest, CopyRefersToSameObject) {
    auto shared = ShrdPtr<Tracked>::make(3);
    WeakPtr<Tracked> first = shared;
    WeakPtr<Tracked> second;
    second = first;
    EXPECT_EQ(first.lock()->value, 3);
    EXPECT_EQ(second.lock()->value, 3);
}

TEST(WeakPtrTest, MoveEmptiesSource) {
    auto shared = ShrdPtr<Tracked>::make(3);
    WeakPtr<Tracked> first = shared;
    WeakPtr<Tracked> moved(std::move(first));
    EXPECT_TRUE(first.expired());
    EXPECT_EQ(moved.lock()->value, 3);

    first = std::move(moved);
    EXPECT_EQ(first.lock()->value, 3);
}

// weak = weak и weak = std::move(weak) не теряют ссылку.
TEST(WeakPtrTest, SelfAssignmentKeepsReference) {
    auto shared = ShrdPtr<Tracked>::make(3);
    WeakPtr<Tracked> weak = shared;
    WeakPtr<Tracked>& alias = weak;
    weak = alias;
    EXPECT_EQ(weak.lock()->value, 3);
    weak = std::move(alias);
    EXPECT_EQ(weak.lock()->value, 3);
}

// Присваивание ShrdPtr и swap переключают ссылку на другой объект.
TEST(WeakPtrTest, ReassignAndSwap) {
    auto three = ShrdPtr<Tracked>::make(3);
    auto four = ShrdPtr<Tracked>::make(4);
    WeakPtr<Tracked> first = three;
    WeakPtr<Tracked> second = three;
    second = four;
    EXPECT_EQ(second.lock()->value, 4);

    first.swap(second);
    EXPECT_EQ(first.lock()->value, 4);
    EXPECT_EQ(second.lock()->value, 3);
}

// Родитель владеет ребёнком, ребёнок ссылается на родителя слабо: цикла нет, всё удаляется.
TEST(WeakPtrTest, BreaksOwnershipCycle) {
    struct Parent;
    struct Kid {
        Tracked value{2};
        WeakPtr<Parent> parent;
    };
    struct Parent {
        Tracked value{1};
        ShrdPtr<Kid> kid;
    };
    auto parent = ShrdPtr<Parent>::make();
    parent->kid = ShrdPtr<Kid>::make();
    parent->kid->parent = parent;
    EXPECT_EQ(parent->kid->parent.lock()->value.value, 1);

    WeakPtr<Kid> kid = parent->kid;
    parent.reset();
    EXPECT_EQ(Tracked::alive, 0);
    EXPECT_TRUE(kid.expired());
}

// Объект держит слабую ссылку на себя: её освобождение внутри деструктора не удаляет блок раньше времени.
TEST(WeakPtrTest, ObjectHoldsWeakReferenceToItself) {
    struct Self {
        Tracked value{1};
        WeakPtr<Self> self;
    };
    auto shared = ShrdPtr<Self>::make();
    shared->self = shared;
    EXPECT_EQ(shared->self.use_count(), 1u);
    shared.reset();
    EXPECT_EQ(Tracked::alive, 0);
}

// Потоки берут lock(), пока последний владелец отпускает объект: каждый удачный lock()
// видит живой объект, объект удаляется ровно один раз.
TEST(WeakPtrThreadTest, LockRacesWithLastRelease) {
    for (int round = 0; round < 200; round++) {
        auto shared = ShrdPtr<Tracked>::make(7);
        WeakPtr<Tracked> weak = shared;
        std::atomic<bool> start{false};
        std::atomic<int> broken{0};
        std::vector<std::thread> workers;
        for (int thread = 0; thread < THREADS; thread++) {
            workers.emplace_back([weak, &start, &broken] {
                while (!start.load()) {} // Все потоки стартуют одновременно с reset() ниже
                for (int step = 0; step < 200; step++) {
                    ShrdPtr<Tracked> locked = weak.lock();
                    if (locked && locked->value != 7) broken++;
                }
            });
        }
        start = true;
        shared.reset();
        for (auto& worker : workers) worker.join();

        EXPECT_EQ(broken.load(), 0);
        EXPECT_EQ(Tracked::alive, 0);
        EXPECT_TRUE(weak.expired());
    }
}

// ---------------------------------------------------------------- проверка самой проверки

// Счётчик видит обычные new / delete: забытый delete был бы пойман после теста.
TEST(LeakCheckTest, CounterSeesRawAllocations) {
    auto live = alloc_snapshot().live();
    int* raw = new int(1);
    EXPECT_EQ(alloc_snapshot().live(), live + 1);
    delete raw;
    EXPECT_EQ(alloc_snapshot().live(), live);
}
