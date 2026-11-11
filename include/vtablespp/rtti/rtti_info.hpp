#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace vtablespp::rtti {

struct rtti_info {
    bool valid = false;

    std::string mangled_class_name;
    std::optional<std::string> class_name;

    std::uint32_t vtable_offset = 0;
    std::uint32_t constructor_displacement = 0;

    bool multiple_inheritance = false;
    bool virtual_inheritance = false;

    std::size_t base_class_count = 0;
};

} // namespace vtablespp::rtti