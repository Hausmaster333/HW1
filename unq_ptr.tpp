#include "unq_ptr.h"
#include <stdexcept>

template <class T>
UnqPtr<T>::UnqPtr(decentral_detail::Record* record,
                  std::function<T*(decentral_detail::Record*)>&& view) noexcept
    : record(record), view(std::move(view)) {}

template <class T>
UnqPtr<T>::UnqPtr() noexcept : record(nullptr) {}

template <class T>
UnqPtr<T>::UnqPtr(UnqPtr<T>&& other) noexcept
    : record(other.record), view(std::move(other.view)) {
    other.record = nullptr;
}

template <class T>
UnqPtr<T>& UnqPtr<T>::operator=(UnqPtr<T>&& other) noexcept {
    if (this == &other) return *this;

    reset();
    record = other.record;
    view = std::move(other.view);
    other.record = nullptr;
    return *this;
}

template <class T>
template <class U, class>
UnqPtr<T>::UnqPtr(UnqPtr<U>&& other) : record(nullptr) {
    if (other.record == nullptr) return;

    std::function<T*(decentral_detail::Record*)> new_view =
        [old_view = other.view](decentral_detail::Record* source) -> T* {
            return old_view(source);
        };

    record = other.record;
    view.swap(new_view);
    other.record = nullptr;
    other.view = nullptr;
}

template <class T>
template <class U, class>
UnqPtr<T>& UnqPtr<T>::operator=(UnqPtr<U>&& other) {
    UnqPtr<T> incoming(std::move(other));
    swap(incoming);
    return *this;
}

template <class T>
template <class... Args>
UnqPtr<T> UnqPtr<T>::make(Args&&... args) {
    std::function<T*(decentral_detail::Record*)> new_view =
        [](decentral_detail::Record* source) -> T* {
            return &static_cast<decentral_detail::ObjectRecord<T>*>(source)->value;
        };
    auto* new_record = new decentral_detail::ObjectRecord<T>(std::forward<Args>(args)...);
    return UnqPtr<T>(new_record, std::move(new_view));
}

template <class T>
UnqPtr<T> UnqPtr<T>::clone() const {
    if (record == nullptr) return UnqPtr<T>();

    auto new_view = view;
    auto* new_record = record->clone();
    return UnqPtr<T>(new_record, std::move(new_view));
}

template <class T>
ShrdPtr<T> UnqPtr<T>::share() const noexcept {
    return ShrdPtr<T>(*this);
}

template <class T>
T* UnqPtr<T>::get() const noexcept {
    return record == nullptr ? nullptr : view(record);
}

template <class T>
T& UnqPtr<T>::operator*() const {
    if (record == nullptr) throw std::logic_error("UnqPtr is empty");
    return *get();
}

template <class T>
T* UnqPtr<T>::operator->() const {
    if (record == nullptr) throw std::logic_error("UnqPtr is empty");
    return get();
}

template <class T>
UnqPtr<T>::operator bool() const noexcept {
    return record != nullptr;
}

template <class T>
size_t UnqPtr<T>::descriptor_count() const noexcept {
    return record == nullptr ? 0 : record->descriptors;
}

template <class T>
void UnqPtr<T>::reset() noexcept {
    if (record == nullptr) return;

    auto* old_record = record;
    record = nullptr;
    view = nullptr;
    old_record->has_owner = false;
    if (old_record->descriptors == 0) delete old_record;
}

template <class T>
void UnqPtr<T>::swap(UnqPtr<T>& other) noexcept {
    std::swap(record, other.record);
    view.swap(other.view);
}

template <class T>
UnqPtr<T>::~UnqPtr() {
    reset();
}

template <class T>
UnqPtr<T[]>::UnqPtr(decentral_detail::ArrayRecord<T>* record) noexcept : record(record) {}

template <class T>
UnqPtr<T[]>::UnqPtr() noexcept : record(nullptr) {}

template <class T>
UnqPtr<T[]>::UnqPtr(UnqPtr<T[]>&& other) noexcept : record(other.record) {
    other.record = nullptr;
}

template <class T>
UnqPtr<T[]>& UnqPtr<T[]>::operator=(UnqPtr<T[]>&& other) noexcept {
    if (this == &other) return *this;

    reset();
    record = other.record;
    other.record = nullptr;
    return *this;
}

template <class T>
UnqPtr<T[]> UnqPtr<T[]>::make_array(size_t count) {
    static_assert(std::is_default_constructible<T>::value,
                  "UnqPtr<T[]>::make_array() requires default-constructible elements");
    if (count == 0) return UnqPtr<T[]>();

    auto* new_record = new decentral_detail::ArrayRecord<T>(count);
    return UnqPtr<T[]>(new_record);
}

template <class T>
UnqPtr<T[]> UnqPtr<T[]>::clone() const {
    if (record == nullptr) return UnqPtr<T[]>();

    return UnqPtr<T[]>(record->clone());
}

template <class T>
ShrdPtr<T[]> UnqPtr<T[]>::share() const noexcept {
    return ShrdPtr<T[]>(*this);
}

template <class T>
T* UnqPtr<T[]>::get() const noexcept {
    return record == nullptr ? nullptr : record->data;
}

template <class T>
T& UnqPtr<T[]>::operator[](size_t index) const {
    if (index >= size()) throw std::out_of_range("UnqPtr array index out of range");
    return record->data[index];
}

template <class T>
UnqPtr<T[]>::operator bool() const noexcept {
    return record != nullptr;
}

template <class T>
size_t UnqPtr<T[]>::size() const noexcept {
    return record == nullptr ? 0 : record->count;
}

template <class T>
size_t UnqPtr<T[]>::descriptor_count() const noexcept {
    return record == nullptr ? 0 : record->descriptors;
}

template <class T>
void UnqPtr<T[]>::reset() noexcept {
    if (record == nullptr) return;

    auto* old_record = record;
    record = nullptr;
    old_record->has_owner = false;
    if (old_record->descriptors == 0) delete old_record;
}

template <class T>
void UnqPtr<T[]>::swap(UnqPtr<T[]>& other) noexcept {
    std::swap(record, other.record);
}

template <class T>
UnqPtr<T[]>::~UnqPtr() {
    reset();
}
