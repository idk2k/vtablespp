#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <type_traits>

#include "vtablespp/abi/abi.hpp"

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
    using entry_type = const void*;

    vtable_view() noexcept = default;

    explicit vtable_view(
        const entry_type* entries,
        std::size_t size
    ) noexcept
        : entries_(entries), size_(size) {}

    /// Reads the table pointer at the beginning of the supplied object
    /// or base subobject, according to the selected ABI backend.
    template <class T>
    [[nodiscard]]
    static vtable_view from_object(
        const T& object,
        std::size_t size
    ) noexcept {
        static_assert(
            std::is_polymorphic_v<T>,
            "vtable_view requires a polymorphic type"
        );

        const void* table = abi::vtable_of(
            static_cast<const void*>(std::addressof(object))
        );

        return vtable_view{
            static_cast<const entry_type*>(table),
            size
        };
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
    entry_type at(std::size_t index) const noexcept {
        return index < size_ ? entries_[index] : nullptr;
    }

    [[nodiscard]]
    std::span<const entry_type> entries() const noexcept {
        return {entries_, size_};
    }

private:
    const entry_type* entries_ = nullptr;
    std::size_t size_ = 0;
};

} // namespace vtablespp