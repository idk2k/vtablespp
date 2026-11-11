#include "vtablespp/rtti/rtti_reader.hpp"

#include <windows.h>
#include <dbghelp.h>

#include <cstring>
#include <string>

#include "vtablespp/abi/abi.hpp"
#include "vtablespp/rtti/complete_object_locator.hpp"
#include "vtablespp/rtti/type_descriptor.hpp"

#include <cstdint>
#include <limits>
#include <string>

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
    const auto* table = static_cast<const void* const*>(vtable);

    if (!abi::read_memory(table - 1, sizeof(const void*), &locator_address)) {
        return false;
    }

    if (locator_address == nullptr) {
        return false;
    }

    return abi::read_memory(locator_address, sizeof(locator), &locator);
}
[[nodiscard]]
bool read_type_name(
    const void* descriptor_address,
    std::string& name
) {
    name.clear();

    if (descriptor_address == nullptr) {
        return false;
    }

    const auto address =
        reinterpret_cast<std::uintptr_t>(descriptor_address);

    constexpr auto header_size = sizeof(type_descriptor_header);
    constexpr auto max_address =
        (std::numeric_limits<std::uintptr_t>::max)();

    if (address > max_address - header_size - kMaxTypeNameLength) {
        return false;
    }

    type_descriptor_header header{};

    if (!abi::read_memory(
            descriptor_address,
            sizeof(header),
            &header
        ) || header.vtable == nullptr) {
        return false;
    }

    const auto name_address = address + header_size;

    std::string result;

    for (std::size_t i = 0; i < kMaxTypeNameLength; ++i) {
        char character{};

        if (!abi::read_memory(
                reinterpret_cast<const void*>(name_address + i),
                sizeof(character),
                &character
            )) {
            return false;
        }

        if (character == '\0') {
            if (result.rfind(".?AV", 0) != 0 &&
                result.rfind(".?AU", 0) != 0) {
                return false;
            }

            name = std::move(result);
            return true;
        }

        result.push_back(character);
    }

    return false;
}
[[nodiscard]]
std::optional<std::string> demangle(
    const std::string& mangled_type_name
) {
    if (mangled_type_name.rfind(".?AV", 0) != 0 &&
        mangled_type_name.rfind(".?AU", 0) != 0) {
        return std::nullopt;
    }

    const std::string decorated_symbol =
        "??_R0" + mangled_type_name.substr(1) + "@8";

    char buffer[4096]{};

    const DWORD length = ::UnDecorateSymbolName(
        decorated_symbol.c_str(),
        buffer,
        static_cast<DWORD>(sizeof(buffer)),
        UNDNAME_COMPLETE
    );

    if (length == 0) {
        return std::nullopt;
    }

    std::string result{buffer, static_cast<std::size_t>(length)};

    const std::string suffix = " `RTTI Type Descriptor'";

    if (result.size() >= suffix.size() &&
        result.compare(
            result.size() - suffix.size(),
            suffix.size(),
            suffix
        ) == 0) {
        result.erase(result.size() - suffix.size());
    }

    if (result.rfind("class ", 0) == 0) {
        result.erase(0, 6);
    } else if (result.rfind("struct ", 0) == 0) {
        result.erase(0, 7);
    }

    return result;
}

} // namespace

rtti_reader::rtti_reader(const void* vtable) noexcept : vtable_(vtable) {}

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

    const auto image_base = locator_address_value - locator.self_rva;

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