#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace vtablespp::symbols {

struct symbol_info {
    std::string name;
    std::optional<std::string> demangled_name;
    std::uint64_t displacement = 0;
    std::optional<std::string> module;
};

} // namespace vtablespp::symbols