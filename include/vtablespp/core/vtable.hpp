#pragma once

#include <span>
#include <optional>
#include <cstddef>
#include <memory>
#include <type_traits>

#include "vtablespp/abi/abi.hpp"
#include "vtablespp/core/slot_info.hpp"

namespace vtablespp {

/// A non-owning, read-only view over virtual method table entries.
///
/// Preconditions:
/// - entries points to at least size readable ABI-specific entries;
/// - the underlying storage remains valid while the view is used.
///
/// The supplied size is trusted, not discovered or validated.
class vtable_view {
public:
    using slot_entry = const void*;

    vtable_view() noexcept = default;

    explicit vtable_view(const slot_entry* entries, std::size_t size) noexcept
        : entries_(entries), size_(size) {}

    /// Reads the table pointer at the beginning of the supplied object
    /// or base subobject, according to the selected ABI backend.
    template <class T>
    [[nodiscard]]
    static vtable_view from_object(const T& object, std::size_t size) noexcept {
        static_assert(
            std::is_polymorphic_v<T>,
            "vtable_view requires a polymorphic type"
        );

        const void* table =
            abi::vtable_of(static_cast<const void*>(std::addressof(object)));

        return vtable_view{static_cast<const slot_entry*>(table), size};
    }

    template <class Base, class Derived>
    [[nodiscard]]
    static vtable_view
    from_base(const Derived& object, std::size_t size) noexcept {
        static_assert(
            std::is_base_of_v<Base, Derived>,
            "Base must be a base class of Derived"
        );

        const Base& base_subobject = static_cast<const Base&>(object);

        const void* table = vtablespp::abi::vtable_of(
            static_cast<const void*>(std::addressof(base_subobject))
        );

        return vtable_view{static_cast<const slot_entry*>(table), size};
    }

    [[nodiscard]]
    bool empty() const noexcept {
        return size_ == 0;
    }

    [[nodiscard]]
    std::size_t size() const noexcept {
        return size_;
    }

    /// Returns nullptr when index is outside the supplied range.
    /// Does not validate whether that range describes a real vftable.
    [[nodiscard]]
    std::optional<slot_entry> at(std::size_t index) const noexcept {
        if (index >= size_)
            return std::nullopt;
        return entries_[index];
    }

    [[nodiscard]]
    std::span<const slot_entry> entries() const noexcept {
        return {entries_, size_};
    }

    [[nodiscard]]
    const slot_entry* vtable_address() const noexcept {
        return entries_;
    }

private:
    const slot_entry* entries_ = nullptr;
    std::size_t size_ = 0;
};

} // namespace vtablespp