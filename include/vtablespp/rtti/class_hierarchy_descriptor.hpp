#pragma once

#include <cstdint>

namespace vtablespp::rtti {

struct class_hierarchy_descriptor {
    std::uint32_t signature = 0;
    std::uint32_t attributes = 0;
    std::uint32_t base_class_count = 0;
    const void* base_class_array = nullptr;
};

} // namespace vtablespp::rtti