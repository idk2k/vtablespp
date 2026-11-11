#pragma once

#include <cstddef>

namespace vtablespp::rtti {

// Только фиксированный заголовок.
// NUL-terminated имя расположено сразу после него.
struct type_descriptor_header {
    const void* vtable;
    const void* spare;
};

static_assert(
    sizeof(type_descriptor_header) == 2 * sizeof(void*)
);

} // namespace vtablespp::rtti