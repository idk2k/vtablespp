#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace vtablespp::abi {

/// MSVC places the vftable pointer as the first field of a polymorphic object.
/// This is an implementation detail of the Microsoft ABI, not a C++ standard guarantee.
inline const void* vtable_of(const void* object) noexcept {
    return *static_cast<const void* const*>(object);
}

/// Returns the address of the vtable slot at the given index.
/// Bounds are the caller's responsibility.
inline const void* vtable_entry(const void* vtable, std::size_t index) noexcept {
    const auto* entries = static_cast<const void* const*>(vtable);
    return entries[index];
}

} // namespace vtablespp::abi