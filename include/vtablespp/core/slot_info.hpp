#pragma once

#include <cstddef>
#include <optional>

namespace vtablespp {

enum class slot_kind {
    direct,          // direct function address
    this_adjustor,   // thunk with this adjustor
    return_adjustor, // thunk with return adjustor
    pure_call,       // slot pointed to __purecall
    unknown
};

enum class direct_evidence {
    none,
    symbol_resolved,
    rtti_confirmed,
    manually_verified
};

struct slot_info {
    slot_kind kind = slot_kind::unknown;
    std::ptrdiff_t this_adjustment = 0;
    std::ptrdiff_t return_adjustment = 0;
    bool is_virtual_thunk = false;
    const void* entry = nullptr;
    std::optional<const void*> final_target;
    bool has_rtti = false;
    direct_evidence evidence;
};

} // namespace vtablespp