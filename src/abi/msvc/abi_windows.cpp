#include "vtablespp/abi/abi.hpp"

#include <Windows.h>
#include <optional>

extern "C" int __cdecl _purecall();

namespace vtablespp::abi {
namespace {

[[nodiscard]]
bool is_readable_protection(DWORD protect) noexcept {
    switch (protect) {
    case PAGE_READONLY:
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
    case PAGE_EXECUTE:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return true;
    default:
        return false;
    }
}

} // namespace

std::optional<const void*> purecall_address() noexcept {
    return static_cast<const void*>(&_purecall);
}

bool read_memory(
    const void* address,
    std::size_t size,
    void* destination
) noexcept {
    if (address == nullptr || destination == nullptr || size == 0) {
        return false;
    }

    MEMORY_BASIC_INFORMATION info{};

    if (::VirtualQuery(address, &info, sizeof(info)) != sizeof(info)) {
        return false;
    }

    if (info.State != MEM_COMMIT || !is_readable_protection(info.Protect)) {
        return false;
    }

    const auto* first = static_cast<const unsigned char*>(address);
    const auto* region_end =
        static_cast<const unsigned char*>(info.BaseAddress) +
        info.RegionSize;

    if (first + size > region_end) {
        return false;
    }

    __try {
        std::memcpy(destination, address, size);
        return true;
    } __except (
        GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
            ? EXCEPTION_EXECUTE_HANDLER
            : EXCEPTION_CONTINUE_SEARCH
    ) {
        return false;
    }
}

bool read_code(
    const void* address,
    std::size_t size,
    void* destination
) noexcept {
    return read_memory(address, size, destination);
}

namespace {

[[nodiscard]]
bool is_executable_protection(DWORD protect) noexcept {
    switch (protect) {
    case PAGE_EXECUTE:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return true;
    default:
        return false;
    }
}

[[nodiscard]]
bool points_to_code(const void* address) noexcept {
    if (address == nullptr) {
        return false;
    }

    MEMORY_BASIC_INFORMATION info{};
    if (::VirtualQuery(address, &info, sizeof(info)) != sizeof(info)) {
        return false;
    }

    if (info.State != MEM_COMMIT ||
        !is_executable_protection(info.Protect)) {
        return false;
    }

    unsigned char first_byte = 0;
    return read_code(address, sizeof(first_byte), &first_byte);
}

}

std::size_t vtable_size(const void* vtable) noexcept {
    if (vtable == nullptr) {
        return 0;
    }

    const auto* entries = static_cast<const void* const*>(vtable);

    MEMORY_BASIC_INFORMATION table_info{};
    if (::VirtualQuery(vtable, &table_info, sizeof(table_info)) !=
        sizeof(table_info)) {
        return 0;
    }

    if (table_info.State != MEM_COMMIT) {
        return 0;
    }

    const auto* table_begin =
        static_cast<const unsigned char*>(table_info.BaseAddress);
    const auto* table_end = table_begin + table_info.RegionSize;
    const auto* current = static_cast<const unsigned char*>(vtable);

    std::size_t size = 0;

    while (current + sizeof(const void*) <= table_end) {
        const void* entry = nullptr;

        if (!read_memory(current, sizeof(entry), &entry)) {
            break;
        }

        if (!points_to_code(entry)) {
            break;
        }

        ++size;
        current += sizeof(const void*);
    }

    return size;
}

} // namespace vtablespp::abi