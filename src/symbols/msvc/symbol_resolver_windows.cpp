#include "vtablespp/symbols/symbol_resolver.hpp"

#include <windows.h>
#include <dbghelp.h>
#include <windows.h>
#include <psapi.h>
#include <dbghelp.h>

#include <array>
#include <utility>

#pragma comment(lib, "Dbghelp.lib")

namespace vtablespp::symbols {
namespace {

constexpr std::size_t kMaxSymbolNameLength = 1024;
constexpr std::size_t kMaxUndecoratedNameLength = 2048;

[[nodiscard]]
std::optional<std::string> undecorate(const char* mangled_name) {
    if (mangled_name == nullptr) {
        return std::nullopt;
    }

    std::array<char, kMaxUndecoratedNameLength> buffer{};

    const auto length = ::UnDecorateSymbolName(
        mangled_name,
        buffer.data(),
        static_cast<DWORD>(buffer.size()),
        UNDNAME_COMPLETE
    );

    if (length == 0) {
        return std::nullopt;
    }

    return std::string{buffer.data(), length};
}

[[nodiscard]]
std::optional<std::string>
module_name_for_address(HANDLE process, DWORD64 address) {
    IMAGEHLP_MODULEW64 module{};
    module.SizeOfStruct = sizeof(module);

    if (!::SymGetModuleInfoW64(process, address, &module)) {
        return std::nullopt;
    }

    const std::wstring wide_name{module.ModuleName};

    if (wide_name.empty()) {
        return std::nullopt;
    }

    const int required_size = ::WideCharToMultiByte(
        CP_UTF8,
        0,
        wide_name.c_str(),
        static_cast<int>(wide_name.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );

    if (required_size <= 0) {
        return std::nullopt;
    }

    std::string utf8_name(static_cast<std::size_t>(required_size), '\0');

    const int converted_size = ::WideCharToMultiByte(
        CP_UTF8,
        0,
        wide_name.c_str(),
        static_cast<int>(wide_name.size()),
        utf8_name.data(),
        required_size,
        nullptr,
        nullptr
    );

    if (converted_size <= 0) {
        return std::nullopt;
    }

    utf8_name.resize(static_cast<std::size_t>(converted_size));

    return utf8_name;
}

} // namespace

symbol_resolver::symbol_resolver() noexcept = default;

symbol_resolver::~symbol_resolver() {
    if (initialized_ && process_ != nullptr) {
        ::SymCleanup(static_cast<HANDLE>(process_));
    }
}

symbol_resolver::symbol_resolver(symbol_resolver&& other) noexcept
    : process_(other.process_), initialized_(other.initialized_) {
    other.process_ = nullptr;
    other.initialized_ = false;
}

symbol_resolver& symbol_resolver::operator=(symbol_resolver&& other) noexcept {
    if (this != &other) {
        if (initialized_ && process_ != nullptr) {
            ::SymCleanup(static_cast<HANDLE>(process_));
        }

        process_ = other.process_;
        initialized_ = other.initialized_;

        other.process_ = nullptr;
        other.initialized_ = false;
    }

    return *this;
}

bool symbol_resolver::initialize() {
    if (initialized_) {
        return true;
    }

    process_ = ::GetCurrentProcess();

    if (!::SymInitialize(static_cast<HANDLE>(process_), nullptr, TRUE)) {
        const DWORD error = ::GetLastError();

        // В редких случаях модуль уже может быть загружен.
        if (error != ERROR_ALREADY_EXISTS) {
            process_ = nullptr;
            return false;
        }
    }

    ::SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);

    initialized_ = true;
    return true;
}

std::optional<symbol_info> symbol_resolver::resolve(const void* address) const {
    if (!initialized_ || address == nullptr) {
        return std::nullopt;
    }

    const auto address_value = reinterpret_cast<DWORD64>(address);

    std::array<char, sizeof(SYMBOL_INFO) + kMaxSymbolNameLength> storage{};

    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage.data());

    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = kMaxSymbolNameLength;

    DWORD64 displacement = 0;

    if (!::SymFromAddr(
            static_cast<HANDLE>(process_),
            address_value,
            &displacement,
            symbol
        )) {
        return std::nullopt;
    }

    symbol_info result;
    result.name = symbol->Name;
    result.displacement = displacement;
    result.module =
        module_name_for_address(static_cast<HANDLE>(process_), address_value);

    result.demangled_name = undecorate(symbol->Name);

    return result;
}

} // namespace vtablespp::symbols