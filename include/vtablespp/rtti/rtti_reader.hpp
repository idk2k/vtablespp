#pragma once

#include <optional>

#include "vtablespp/rtti/rtti_info.hpp"

namespace vtablespp::rtti {

class rtti_reader {
public:
    explicit rtti_reader(const void* vtable) noexcept;

    [[nodiscard]]
    std::optional<rtti_info> read() const;

private:
    const void* vtable_ = nullptr;
};

} // namespace vtablespp::rtti