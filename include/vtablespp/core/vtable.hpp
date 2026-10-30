#pragma once

#include <cstddef>
#include <span>
#include <string_view>
#include <type_traits>

#include "vtablespp/abi/abi.hpp"

namespace vtablespp {

/// A non-owning view over a compiler-generated virtual method table.
///
/// The library deliberately does not infer the table length: a valid upper
/// bound must be supplied by the caller or derived from a known class layout.
class vtable_view {
public:
    using entry_type = const void*;

    vtable_view() noexcept = default;

    explicit vtable_view(entry_type* entries, std::size_t size) noexcept
        : entries_(entries), size_(size) {}

    template <class T>
    static vtable_view from_object(const T& object, std::size_t size) noexcept {
        static_assert(std::is_polymorphic_v<T>);
        const void* table = abi::vtable_of(static_cast<const void*>(&object));
        return vtable_view{static_cast<entry_type*>(const_cast<void*>(table)), size};
    }

    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    [[nodiscard]] entry_type at(std::size_t index) const noexcept {
        return index < size_ ? entries_[index] : nullptr;
    }

    [[nodiscard]] std::span<const entry_type> entries() const noexcept {
        return {entries_, size_};
    }

private:
    entry_type* entries_ = nullptr;
    std::size_t size_ = 0;
};

} // namespace vtablespp