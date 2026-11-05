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

bool read_code(
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

    if (info.State != MEM_COMMIT) {
        return false;
    }

    if (!is_readable_protection(info.Protect)) {
        return false;
    }

    const auto* first =
        static_cast<const unsigned char*>(address);
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

} // namespace vtablespp::abi