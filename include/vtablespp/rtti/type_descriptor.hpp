#pragma once

#include <cstddef>

namespace vtablespp::rtti {

struct type_descriptor {
    const void* vtable = nullptr;
    const void* spare = nullptr;
    const char* mangled_name = nullptr;
};

} // namespace vtablespp::rtti