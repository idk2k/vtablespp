#include "vtablespp/rtti/rtti_reader.hpp"

#include <windows.h>
#include <dbghelp.h>

#include <cstring>
#include <string>

#include "vtablespp/abi/abi.hpp"
#include "vtablespp/rtti/complete_object_locator.hpp"
#include "vtablespp/rtti/type_descriptor.hpp"

#pragma comment(lib, "Dbghelp.lib")

namespace vtablespp::rtti {
namespace {

constexpr std::uint32_t kRelativeLocatorSignature = 1;
constexpr std::size_t kMaxTypeNameLength = 1024;

[[nodiscard]]
bool read_locator(
    const void* vtable,
    complete_object_locator& locator,
    const void*& locator_address
) noexcept {
    const auto* table =
        static_cast<const void* const*>(vtable);

    if (!abi::read_memory(
            table - 1,
            sizeof(const void*),
            &locator_address
        )) {
        return false;
    }

    if (locator_address == nullptr) {
        return false;
    }

    return abi::read_memory(
        locator_address,
        sizeof(locator),
        &locator
    );
}

[[nodiscard]]
bool read_type_name(
    const void* descriptor_address,
    std::string& name
) {
    const auto* descriptor =
        static_cast<const type_descriptor*>(descriptor_address);

    if (descriptor->mangled_name == nullptr) {
        return false;
    }

    name.reserve(kMaxTypeNameLength);

    for (std::size_t i = 0; i < kMaxTypeNameLength; ++i) {
        const char character = descriptor->mangled_name[i];

        if (character == '\0') {
            return !name.empty();
        }

        name.push_back(character);
    }

    return false;
}

[[nodiscard]]
std::optional<std::string> demangle(const std::string& mangled_name) {
    if (mangled_name.size() < 2 || mangled_name.front() != '.') {
        return std::nullopt;
    }

    const char* undecorated_input = mangled_name.c_str() + 1;

    char buffer[1024]{};

    const auto length = ::UnDecorateSymbolName(
        undecorated_input,
        buffer,
        static_cast<DWORD>(sizeof(buffer)),
        UNDNAME_COMPLETE
    );

    if (length == 0) {
        return std::nullopt;
    }

    return std::string{buffer, static_cast<std::size_t>(length)};
}

} // namespace

rtti_reader::rtti_reader(const void* vtable) noexcept
    : vtable_(vtable) {}

std::optional<rtti_info> rtti_reader::read() const {
    if (vtable_ == nullptr) {
        return std::nullopt;
    }

    complete_object_locator locator{};
    const void* locator_address = nullptr;

    if (!read_locator(vtable_, locator, locator_address)) {
        return std::nullopt;
    }

    if (locator.signature != kRelativeLocatorSignature) {
        return std::nullopt;
    }

    const auto locator_address_value =
        reinterpret_cast<std::uintptr_t>(locator_address);

    const auto image_base =
        locator_address_value - locator.self_rva;

    const auto type_descriptor_address =
        image_base + locator.type_descriptor_rva;

    std::string mangled_name;

    if (!read_type_name(
            reinterpret_cast<const void*>(type_descriptor_address),
            mangled_name
        )) {
        return std::nullopt;
    }

    rtti_info result;
    result.valid = true;
    result.mangled_class_name = mangled_name;
    result.class_name = demangle(mangled_name);
    result.vtable_offset = locator.vtable_offset;
    result.constructor_displacement = locator.constructor_displacement;

    return result;
}

} // namespace vtablespp::rtti