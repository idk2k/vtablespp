#pragma once

#include <cstddef>
#include <string_view>
#include <typeinfo>

namespace vtablespp {

/// Static description of a class participating in a vtable layout.
class class_info {
public:
    constexpr class_info() noexcept = default;

    constexpr class_info(std::string_view name, std::size_t vtable_slots) noexcept
        : name_(name), vtable_slots_(vtable_slots) {}

    [[nodiscard]] constexpr std::string_view name() const noexcept { return name_; }
    [[nodiscard]] constexpr std::size_t vtable_slots() const noexcept { return vtable_slots_; }

private:
    std::string_view name_;
    std::size_t vtable_slots_ = 0;
};

} // namespace vtablespp