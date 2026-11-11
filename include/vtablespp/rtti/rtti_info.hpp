#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace vtablespp::rtti {

struct base_class_info {
    std::string mangled_name;
    std::optional<std::string> class_name;

    std::ptrdiff_t member_displacement = 0;
    std::ptrdiff_t virtual_base_pointer_displacement = 0;
    std::ptrdiff_t virtual_table_displacement = 0;

    bool is_virtual = false;
};

struct rtti_info {
    bool valid = false;

    std::string mangled_class_name;
    std::optional<std::string> class_name;

    std::uint32_t vtable_offset = 0;
    std::uint32_t constructor_displacement = 0;

    bool multiple_inheritance = false;
    bool virtual_inheritance = false;

    std::size_t base_class_count = 0;
    std::vector<base_class_info> base_classes;
};

} // namespace vtablespp::rtti