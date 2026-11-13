#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
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

} // namespace detail

template <class Signature>
class slot_context;

template <class Return, class... Args>
class slot_context<Return(Args...)> {
public:
    using function_type =
        typename detail::slot_hook_function<Return(Args...)>::type;

    explicit slot_context(const void* original_entry) noexcept
        : original_entry_(original_entry) {}

    template <class T, class... CallArgs>
    decltype(auto) call_original(T& object, CallArgs&&... call_args) const {
        static_assert(
            std::is_polymorphic_v<std::remove_cv_t<T>>,
            "call_original requires a polymorphic type"
        );

        if (original_entry_ == nullptr) {
            throw std::runtime_error("original vtable slot is not available");
        }

        const auto function =
            reinterpret_cast<function_type>(const_cast<void*>(original_entry_));

        return function(
            detail::mutable_object_address(object),
            std::forward<CallArgs>(call_args)...
        );
    }

    template <class... CallArgs>
    decltype(auto)
    call_original_raw(void* object, CallArgs&&... call_args) const {
        if (original_entry_ == nullptr) {
            throw std::runtime_error("original vtable slot is not available");
        }

        const auto function =
            reinterpret_cast<function_type>(const_cast<void*>(original_entry_));

        return function(object, std::forward<CallArgs>(call_args)...);
    }

    [[nodiscard]]
    const void* original_entry() const noexcept {
        return original_entry_;
    }

private:
    const void* original_entry_;
};

template <class Signature>
class stateful_slot_hook;

template <class Return, class... Args>
class stateful_slot_hook<Return(Args...)> {
public:
    using function_type =
        typename detail::slot_hook_function<Return(Args...)>::type;

    using context_type = slot_context<Return(Args...)>;

    using callback_type = std::function<Return(context_type&, void*, Args...)>;

    stateful_slot_hook() noexcept = default;

    stateful_slot_hook(const stateful_slot_hook&) = delete;
    stateful_slot_hook& operator=(const stateful_slot_hook&) = delete;

    stateful_slot_hook(stateful_slot_hook&& other) noexcept
        : table_(other.table_), index_(other.index_), object_(other.object_),
          original_(other.original_), installed_(other.installed_) {
        other.table_ = {};
        other.index_ = 0;
        other.object_ = nullptr;
        other.original_ = nullptr;
        other.installed_ = false;
    }

    stateful_slot_hook& operator=(stateful_slot_hook&& other) noexcept {
        if (this != &other) {
            uninstall();

            table_ = other.table_;
            index_ = other.index_;
            object_ = other.object_;
            original_ = other.original_;
            installed_ = other.installed_;

            other.table_ = {};
            other.index_ = 0;
            other.object_ = nullptr;
            other.original_ = nullptr;
            other.installed_ = false;
        }

        return *this;
    }

    ~stateful_slot_hook() {
        uninstall();
    }

    template <class T, class Callback>
    bool install(
        const vtable_view& table,
        std::size_t index,
        T& object,
        Callback&& callback
    ) {
        static_assert(
            std::is_polymorphic_v<std::remove_cv_t<T>>,
            "stateful_slot_hook requires a polymorphic type"
        );

        std::scoped_lock lock{mutex()};

        if (installed_) {
            return false;
        }

        void* object_address = detail::mutable_object_address(object);

        auto& contexts = hook_contexts();

        if (contexts.find(object_address) != contexts.end()) {
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

        auto storage = std::make_shared<context_storage>();

        storage->original_entry = *entry;
        storage->hook = std::forward<Callback>(callback);

        fallback_original_entry = *entry;
        fallback_vtable_address = table.vtable_address();
        fallback_slot_index = index;

        const void* thunk = reinterpret_cast<const void*>(&thunk_entry);

        if (!abi::write_memory(
                table.vtable_address() + index,
                sizeof(const void*),
                &thunk
            )) {
            return false;
        }

        contexts.emplace(object_address, std::move(storage));

        table_ = table;
        index_ = index;
        object_ = object_address;
        original_ = *entry;
        installed_ = true;

        return true;
    }

    void uninstall() {
        std::scoped_lock lock{mutex()};

        if (!installed_) {
            return;
        }

        const auto current_table_address = table_.vtable_address();
        const auto current_index = index_;

        auto& contexts = hook_contexts();
        const auto found = contexts.find(object_);

        const void* original = original_;

        if (found != contexts.end()) {
            original = found->second->original_entry;
            contexts.erase(found);
        }

        if (abi::write_memory(
                current_table_address + current_index,
                sizeof(const void*),
                &original
            )) {
            if (fallback_vtable_address == current_table_address &&
                fallback_slot_index == current_index) {
                fallback_original_entry = nullptr;
                fallback_vtable_address = nullptr;
                fallback_slot_index = 0;
            }

            installed_ = false;
            original_ = nullptr;
            object_ = nullptr;
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

private:
    struct context_storage {
        const void* original_entry = nullptr;
        const void* vtable_address = nullptr;
        std::size_t slot_index = 0;
        callback_type hook;
    };

    using context_ptr = std::shared_ptr<context_storage>;

    using context_map = std::unordered_map<void*, context_ptr>;

    static context_map& hook_contexts() {
        static context_map instances;
        return instances;
    }

    static std::mutex& mutex() {
        static std::mutex instance;
        return instance;
    }

    static const void* fallback_original_entry;
    static const void* fallback_vtable_address;
    static std::size_t fallback_slot_index;

    static Return thunk_entry(void* object, Args... args) {
        context_ptr context;

        {
            std::scoped_lock lock{mutex()};

            const auto found = hook_contexts().find(object);

            if (found != hook_contexts().end()) {
                context = found->second;
            }
        }

        if (!context) {
            if (fallback_original_entry == nullptr) {
                throw std::runtime_error(
                    "stateful hook fallback original entry is not available"
                );
            }

            const auto function = reinterpret_cast<function_type>(
                const_cast<void*>(fallback_original_entry)
            );

            return function(object, args...);
        }

        context_type context_view{context->original_entry};

        return context->hook(context_view, object, args...);
    }

    vtable_view table_;
    std::size_t index_ = 0;
    void* object_ = nullptr;
    const void* original_ = nullptr;
    bool installed_ = false;
};

template <class Return, class... Args>
const void* stateful_slot_hook<Return(Args...)>::fallback_original_entry =
    nullptr;

template <class Return, class... Args>
const void* stateful_slot_hook<Return(Args...)>::fallback_vtable_address =
    nullptr;

template <class Return, class... Args>
std::size_t stateful_slot_hook<Return(Args...)>::fallback_slot_index = 0;

} // namespace vtablespp