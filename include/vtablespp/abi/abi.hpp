#pragma once

#include "vtablespp/config.hpp"

#if defined(VTABLESPP_ABI_MSVC)
#  include "vtablespp/abi/msvc.hpp"
#else
#  include "vtablespp/abi/unknown.hpp"
#endif

namespace vtablespp {

/// Returns the canonical vtable pointer stored in a polymorphic object.
/// The exact location is ABI-dependent.
template <class T>
const void* vtable_of(const T& object) noexcept {
    static_assert(std::is_polymorphic_v<T>,
                  "vtable_of requires a polymorphic type");
    return abi::vtable_of(static_cast<const void*>(&object));
}

} // namespace vtablespp