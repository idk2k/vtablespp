#pragma once

#include <cstddef>
#include <optional>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

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

    direct_evidence evidence = direct_evidence::none;

    std::optional<std::string> symbol_name;
    std::optional<std::string> demangled_name;
    std::uint64_t symbol_displacement = 0;
    std::optional<std::string> module;

    bool has_rtti = false;
    std::optional<std::string> rtti_class_name;
    std::uint32_t rtti_vtable_offset = 0;
};

} // namespace vtablespp