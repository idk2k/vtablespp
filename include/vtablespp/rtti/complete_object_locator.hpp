#pragma once

#include <cstdint>

namespace vtablespp::rtti {

struct complete_object_locator {
    std::uint32_t signature = 0;
    std::uint32_t vtable_offset = 0;
    std::uint32_t constructor_displacement = 0;

    std::uint32_t type_descriptor_rva = 0;
    std::uint32_t class_hierarchy_rva = 0;
    std::uint32_t self_rva = 0;
};

} // namespace vtablespp::rtti