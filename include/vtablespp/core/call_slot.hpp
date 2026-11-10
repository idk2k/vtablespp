#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

#include "vtablespp/abi/abi.hpp"
#include "vtablespp/core/slot_info.hpp"

namespace vtablespp {

namespace detail {

template <class Signature>
struct slot_function;

template <class Return, class... Args>
struct slot_function<Return(Args...)> {
    using type = Return (*)(void*, Args...);
};

template <class Return, class... Args>
struct slot_function<Return(Args...) noexcept> {
    using type = Return (*)(void*, Args...) noexcept;
};

template <class T>
[[nodiscard]]
void* mutable_object_address(T& object) noexcept {
    return const_cast<void*>(static_cast<const void*>(std::addressof(object)));
}

template <class T>
[[nodiscard]]
const void* const_object_address(const T& object) noexcept {
    return static_cast<const void*>(std::addressof(object));
}

} // namespace detail

template <class Signature, class T, class... Args>
decltype(auto) call_slot(
    T& object,
    std::size_t index,
    Args&&... args
) {
    static_assert(
        std::is_polymorphic_v<std::remove_cv_t<T>>,
        "call_slot requires a polymorphic type"
    );

    using function_type = typename detail::slot_function<Signature>::type;

    const void* vtable = abi::vtable_of(
        detail::mutable_object_address(object)
    );

    const void* entry = abi::vtable_entry(vtable, index);

    if (entry == nullptr) {
        throw std::runtime_error("vtable slot is null");
    }

    const auto purecall = abi::purecall_address();

    if (purecall.has_value() && entry == *purecall) {
        throw std::runtime_error("vtable slot is a pure virtual function");
    }

    const auto function = reinterpret_cast<function_type>(
        const_cast<void*>(entry)
    );

    return function(
        detail::mutable_object_address(object),
        std::forward<Args>(args)...
    );
}

template <class Signature, class T, class... Args>
decltype(auto) call_slot(
    const T& object,
    std::size_t index,
    Args&&... args
) {
    static_assert(
        std::is_polymorphic_v<std::remove_cv_t<T>>,
        "call_slot requires a polymorphic type"
    );

    using function_type = typename detail::slot_function<Signature>::type;

    const void* vtable = abi::vtable_of(
        detail::const_object_address(object)
    );

    const void* entry = abi::vtable_entry(vtable, index);

    if (entry == nullptr) {
        throw std::runtime_error("vtable slot is null");
    }

    const auto purecall = abi::purecall_address();

    if (purecall.has_value() && entry == *purecall) {
        throw std::runtime_error("vtable slot is a pure virtual function");
    }

    const auto function = reinterpret_cast<function_type>(
        const_cast<void*>(entry)
    );

    return function(
        detail::const_object_address(object),
        std::forward<Args>(args)...
    );
}

} // namespace vtablespp