#pragma once

#include <cstddef>

namespace vtablespp::abi {

inline const void* vtable_of(const void*) noexcept {
    return nullptr;
}

inline const void* vtable_entry(const void*, std::size_t) noexcept {
    return nullptr;
}

} // namespace vtablespp::abi