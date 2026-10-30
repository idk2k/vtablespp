#pragma once

#include <string_view>

namespace vtablespp::rtti {

/// Compiler-agnostic placeholder for RTTI inspection.
///
/// MSVC implementation will expose:
/// - Complete Object Locator
/// - Class Hierarchy Descriptor
/// - Base Class Descriptor
/// - Type Descriptor
struct type_name_result {
    std::string_view name;
    bool valid = false;
};

} // namespace vtablespp::rtti