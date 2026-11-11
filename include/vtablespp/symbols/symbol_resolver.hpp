#pragma once

#include <optional>

#include "vtablespp/symbols/symbol_info.hpp"

namespace vtablespp::symbols {

class symbol_resolver {
public:
    symbol_resolver() noexcept;
    ~symbol_resolver();

    symbol_resolver(const symbol_resolver&) = delete;
    symbol_resolver& operator=(const symbol_resolver&) = delete;

    symbol_resolver(symbol_resolver&&) noexcept;
    symbol_resolver& operator=(symbol_resolver&&) noexcept;

    [[nodiscard]] bool initialize();

    [[nodiscard]] std::optional<symbol_info> resolve(
        const void* address
    ) const;

private:
    void* process_ = nullptr;
    bool initialized_ = false;
};

} // namespace vtablespp::symbols