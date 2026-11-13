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

template <class Signature>
struct slot_hook_callback;

template <class Return, class... Args>
struct slot_hook_callback<Return(Args...)> {
    using type =
        Return(slot_context<Return(Args...)>&, void*, Args...);
};

template <class Return, class... Args>
struct slot_hook_callback<Return(Args...) noexcept> {
    using type =
        Return(slot_context<Return(Args...)>&, void*, Args...);
};

}

template <class Signature>
class stateful_slot_hook;

template <class Signature>
class slot_context {
public:
    using function_type = typename detail::slot_hook_function<Signature>::type;

    slot_context(const void* original_entry) noexcept
        : original_entry_(original_entry) {}

    template <class T, class... Args>
    decltype(auto) call_original(T& object, Args&&... args) const {
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
            std::forward<Args>(args)...
        );
    }

    [[nodiscard]]
    const void* original_entry() const noexcept {
        return original_entry_;
    }

private:
    const void* original_entry_;
};

template <class Signature>
class stateful_slot_hook {
public:
    using function_type = typename detail::slot_hook_function<Signature>::type;

    using context_type = slot_context<Signature>;

    using callback_type =
        std::function<typename detail::slot_hook_callback<Signature>::type>;

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

        auto& contexts = contexts();

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

        auto context = std::make_shared<context_type>(*entry);

        context->hook = std::forward<Callback>(callback);

        const void* thunk =
            reinterpret_cast<const void*>(&stateful_slot_hook::thunk);

        if (!abi::write_memory(
                table.vtable_address() + index,
                sizeof(const void*),
                &thunk
            )) {
            return false;
        }

        contexts.emplace(object_address, std::move(context));

        originals()[{table.vtable_address(), index}] = *entry;

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

        auto& contexts = contexts();
        auto context = contexts.find(object_);

        const void* original = original_;

        if (context != contexts.end()) {
            original = context->second->original_entry();
            contexts.erase(context);
        }

        if (abi::write_memory(
                table_.vtable_address() + index_,
                sizeof(const void*),
                &original
            )) {
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
        callback_type hook;
    };

    using context_ptr = std::shared_ptr<context_storage>;

    using context_map = std::unordered_map<void*, context_ptr>;

    using original_key = std::pair<const void*, std::size_t>;

    using original_map = std::unordered_map<original_key, const void*>;

    struct original_key_hash {
        std::size_t operator()(const original_key& key) const noexcept {
            const auto left = reinterpret_cast<std::uintptr_t>(key.first);
            const auto right = static_cast<std::uintptr_t>(key.second);

            return left ^ (right << 1);
        }
    };

    static context_map& contexts() {
        static context_map instances;
        return instances;
    }

    static original_map& originals() {
        static original_map entries;
        return entries;
    }

    static std::mutex& mutex() {
        static std::mutex instance;
        return instance;
    }

    static Return thunk(void* object, Args... args) {
        return dispatch(object, std::forward<Args>(args)...);
    }

    static Return dispatch(void* object, Args... args) {
        context_ptr context;

        {
            std::scoped_lock lock{mutex()};
            const auto found = contexts().find(object);

            if (found != contexts().end()) {
                context = found->second;
            }
        }

        if (!context) {
            throw std::runtime_error("stateful hook context was not found");
        }

        context_type context_view{context->original_entry};

        return context->hook(context_view, object, args...);
    }
};