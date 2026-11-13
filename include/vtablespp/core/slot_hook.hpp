#pragma once

#include <cstddef>
#include <mutex>
#include <type_traits>
#include <utility>

#include "vtablespp/abi/abi.hpp"
#include "vtablespp/core/call_slot.hpp"
#include "vtablespp/core/slot_info.hpp"
#include "vtablespp/core/vtable.hpp"

namespace vtablespp {

namespace detail {

template <class Signature>
struct slot_hook_function;

template <class Return, class... Args>
struct slot_hook_function<Return(Args...)> {
    using type = Return (*)(void*, Args...);
};

template <class Return, class... Args>
struct slot_hook_function<Return(Args...) noexcept> {
    using type = Return (*)(void*, Args...) noexcept;
};

}

template <class Signature>
class slot_hook {
public:
    using function_type =
        typename detail::slot_hook_function<Signature>::type;

    slot_hook() noexcept = default;

    slot_hook(const slot_hook&) = delete;
    slot_hook& operator=(const slot_hook&) = delete;

    slot_hook(slot_hook&& other) noexcept
        : table_(other.table_),
          index_(other.index_),
          original_(other.original_),
          installed_(other.installed_) {
        other.table_ = {};
        other.index_ = 0;
        other.original_ = nullptr;
        other.installed_ = false;
    }

    slot_hook& operator=(slot_hook&& other) noexcept {
        if (this != &other) {
            uninstall();

            table_ = other.table_;
            index_ = other.index_;
            original_ = other.original_;
            installed_ = other.installed_;

            other.table_ = {};
            other.index_ = 0;
            other.original_ = nullptr;
            other.installed_ = false;
        }

        return *this;
    }

    ~slot_hook() {
        uninstall();
    }

    [[nodiscard]]
    bool install(
        const vtable_view& table,
        std::size_t index,
        function_type replacement
    ) {
        std::scoped_lock lock{mutex_};

        if (installed_ || replacement == nullptr) {
            return false;
        }

        const auto entry = table.at(index);
        if (!entry.has_value() || *entry == nullptr) {
            return false;
        }

        const auto purecall = abi::purecall_address();
        if (purecall.has_value() && *entry == *purecall) {
            return false;
        }

        if (!abi::write_memory(
                table.vtable_address() + index,
                sizeof(const void*),
                &replacement
            )) {
            return false;
        }

        table_ = table;
        index_ = index;
        original_ = *entry;
        installed_ = true;

        return true;
    }

    void uninstall() {
        std::scoped_lock lock{mutex_};

        if (!installed_) {
            return;
        }

        const void* original = original_;

        if (abi::write_memory(
                table_.vtable_address() + index_,
                sizeof(const void*),
                &original
            )) {
            installed_ = false;
            original_ = nullptr;
            table_ = {};
            index_ = 0;
        }
    }

    [[nodiscard]]
    bool installed() const noexcept {
        return installed_;
    }

    [[nodiscard]]
    const void* original_entry() const noexcept {
        return original_;
    }

    template <class T, class... Args>
    decltype(auto) call_original(T& object, Args&&... args) const {
        static_assert(
            std::is_polymorphic_v<std::remove_cv_t<T>>,
            "call_original requires a polymorphic type"
        );

        if (original_ == nullptr) {
            throw std::runtime_error("original vtable slot is not installed");
        }

        const auto function =
            reinterpret_cast<function_type>(const_cast<void*>(original_));

        return function(
            detail::mutable_object_address(object),
            std::forward<Args>(args)...
        );
    }

private:
    vtable_view table_;
    std::size_t index_ = 0;
    const void* original_ = nullptr;
    bool installed_ = false;

    static std::mutex mutex_;
};

template <class Signature>
std::mutex slot_hook<Signature>::mutex_;

}