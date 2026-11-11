#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <optional>

#include "vtablespp/abi/abi.hpp"

namespace vtablespp::abi {

/// MSVC places the vftable pointer as the first field of a polymorphic object.
/// This is an implementation detail of the Microsoft ABI, not a C++ standard
/// guarantee.
inline const void* vtable_of(const void* object) noexcept {
    return *static_cast<const void* const*>(object);
}

/// Returns the address of the vtable slot at the given index.
/// Bounds are the caller's responsibility.
inline const void*
vtable_entry(const void* vtable, std::size_t index) noexcept {
    const auto* entries = static_cast<const void* const*>(vtable);
    return entries[index];
}

/// Returns the address of the MSVC pure-call handler, if it can be resolved.
[[nodiscard]]
std::optional<const void*> purecall_address() noexcept;

/// Safely copies executable bytes for pattern inspection.
///
/// Returns false if the requested range cannot be read.
[[nodiscard]]
bool read_code(
    const void* address,
    std::size_t size,
    void* destination
) noexcept;

[[nodiscard]]
bool read_memory(
    const void* address,
    std::size_t size,
    void* destination
) noexcept;

} // namespace vtablespp::abi