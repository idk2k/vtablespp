#pragma once

#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "vtablespp/abi/abi.hpp"
#include "vtablespp/core/call_slot.hpp"
#include "vtablespp/core/slot_info.hpp"
#include "vtablespp/core/vtable.hpp"

#if defined(_WIN32) && defined(_WIN64)
#include <Windows.h>
#define VTABLESPP_STATEFUL_HOOK_SUPPORTED 1
#else
#define VTABLESPP_STATEFUL_HOOK_SUPPORTED 0
#endif

namespace vtablespp {

template <class Signature>
class stateful_slot_hook;

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

inline std::mutex& hook_registry_mutex() noexcept {
    static std::mutex mutex;
    return mutex;
}

inline std::set<const void*>& hooked_objects() noexcept {
    static std::set<const void*> objects;
    return objects;
}

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

    stateful_slot_hook(stateful_slot_hook&& other) noexcept {
        steal_from(other);
    }

    stateful_slot_hook& operator=(stateful_slot_hook&& other) noexcept {
        if (this != &other) {
            uninstall();
            steal_from(other);
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
        static_assert(
            VTABLESPP_STATEFUL_HOOK_SUPPORTED,
            "stateful_slot_hook currently requires 64-bit Windows"
        );

        std::scoped_lock registry_lock{detail::hook_registry_mutex()};

        if (installed_) {
            return false;
        }

        void* object_address = detail::mutable_object_address(object);
        auto& objects = detail::hooked_objects();

        std::printf("[hook] install begin, object=%p\n", object_address);
        std::fflush(stdout);

        if (objects.find(object_address) != objects.end()) {
            return false;
        }

        const void* current_vtable = abi::vtable_of(object_address);

        if (current_vtable == nullptr) {
            return false;
        }

        const std::size_t slot_count = abi::vtable_size(current_vtable);
        std::printf(
            "[hook] install vtable=%p slots=%zu index=%zu\n",
            current_vtable,
            slot_count,
            index
        );
        std::fflush(stdout);

        if (index >= slot_count) {
            return false;
        }

        const auto current_entries =
            static_cast<const void* const*>(current_vtable);

        const void* original_entry = current_entries[index];

        if (original_entry == nullptr) {
            return false;
        }

        const auto purecall = abi::purecall_address();

        if (purecall.has_value() && original_entry == *purecall) {
            return false;
        }

        unsigned char* cloned_vtable =
            allocate_cloned_vtable(current_vtable, slot_count);

        std::printf(
            "[hook] install clone=%p\n",
            static_cast<void*>(cloned_vtable)
        );
        std::fflush(stdout);

        if (cloned_vtable == nullptr) {
            return false;
        }

        const void** cloned_entries =
            reinterpret_cast<const void**>(cloned_vtable + sizeof(const void*));

        auto storage = std::make_shared<context_storage>();
        storage->original_entry = original_entry;
        storage->hook = std::forward<Callback>(callback);

        cloned_entries[index] = reinterpret_cast<const void*>(&dispatch);
        std::printf("[hook] install slot patched\n");
        std::fflush(stdout);

        const void* new_vtable_address = cloned_entries;

        if (!abi::write_memory(
                object_address,
                sizeof(const void*),
                &new_vtable_address
            )) {
            free_cloned_vtable(cloned_vtable);
            return false;
        }

        std::printf("[hook] install vptr switched\n");
        std::fflush(stdout);

        contexts().emplace(cloned_entries, storage);

        objects.insert(object_address);

        context_ = std::move(storage);
        cloned_vtable_ = cloned_vtable;
        cloned_entries_ = cloned_entries;
        object_ = object_address;
        previous_vtable_ = current_vtable;
        original_ = original_entry;
        installed_ = true;

        std::printf("[hook] install complete\n");
        std::fflush(stdout);

        return true;
    }

    void uninstall() {
        std::scoped_lock registry_lock{detail::hook_registry_mutex()};

        if (!installed_) {
            return;
        }

        const void* current_vtable = nullptr;

        const bool object_still_uses_clone =
            abi::read_memory(object_, sizeof(const void*), &current_vtable) &&
            current_vtable == cloned_entries_;

        if (object_still_uses_clone) {
            const void* restored_vtable = previous_vtable_;

            abi::write_memory(object_, sizeof(const void*), &restored_vtable);

            detail::hooked_objects().erase(object_);
        }

        contexts().erase(cloned_entries_);
        free_cloned_vtable(cloned_vtable_);

        context_.reset();
        cloned_vtable_ = nullptr;
        cloned_entries_ = nullptr;
        object_ = nullptr;
        previous_vtable_ = nullptr;
        original_ = nullptr;
        installed_ = false;
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

    static std::unordered_map<const void*, context_ptr>& contexts() {
        static std::unordered_map<const void*, context_ptr> contexts;
        return contexts;
    }

    static Return dispatch(void* object, Args... args) {
        std::printf("[hook] dispatch enter, object=%p\n", object);
        std::fflush(stdout);

        context_ptr context;

        std::printf("[hook] dispatch before lock\n");
        std::fflush(stdout);

        {
            std::scoped_lock lock{detail::hook_registry_mutex()};

            std::printf("[hook] dispatch lock acquired\n");
            std::fflush(stdout);

            const void* vtable = abi::vtable_of(object);

            std::printf("[hook] dispatch vtable=%p\n", vtable);
            std::fflush(stdout);

            const auto found = contexts().find(vtable);

            std::printf(
                "[hook] dispatch lookup %s\n",
                found != contexts().end() ? "found" : "miss"
            );
            std::fflush(stdout);

            if (found == contexts().end()) {
                std::printf(
                    "[hook] dispatch context miss, vtable=%p\n",
                    vtable
                );
                std::fflush(stdout);

                throw std::runtime_error("stateful hook context was not found");
            }

            context = found->second;

            std::printf(
                "[hook] dispatch context acquired, original=%p\n",
                context->original_entry
            );
            std::fflush(stdout);
        }

        std::printf("[hook] dispatch lock released\n");
        std::fflush(stdout);

        context_type context_view(context->original_entry);

        std::printf("[hook] dispatch calling hook\n");
        std::fflush(stdout);

        return context->hook(context_view, object, std::forward<Args>(args)...);
    }

    static unsigned char*
    allocate_cloned_vtable(const void* current_vtable, std::size_t slot_count) {
        const std::size_t bytes = (slot_count + 1) * sizeof(const void*);

        void* memory = VirtualAlloc(
            nullptr,
            bytes,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE
        );

        if (memory == nullptr) {
            return nullptr;
        }

        auto* cloned = static_cast<unsigned char*>(memory);

        std::memcpy(
            cloned,
            static_cast<const unsigned char*>(current_vtable) -
                sizeof(const void*),
            bytes
        );

        return cloned;
    }

    static void free_cloned_vtable(unsigned char* cloned_vtable) noexcept {
        if (cloned_vtable != nullptr) {
            VirtualFree(cloned_vtable, 0, MEM_RELEASE);
        }
    }

    void steal_from(stateful_slot_hook& other) noexcept {
        context_ = std::move(other.context_);
        cloned_vtable_ = other.cloned_vtable_;
        cloned_entries_ = other.cloned_entries_;
        object_ = other.object_;
        previous_vtable_ = other.previous_vtable_;
        original_ = other.original_;
        installed_ = other.installed_;

        other.cloned_vtable_ = nullptr;
        other.cloned_entries_ = nullptr;
        other.object_ = nullptr;
        other.previous_vtable_ = nullptr;
        other.original_ = nullptr;
        other.installed_ = false;
    }

    context_ptr context_;
    unsigned char* cloned_vtable_ = nullptr;
    const void** cloned_entries_ = nullptr;
    void* object_ = nullptr;
    const void* previous_vtable_ = nullptr;
    const void* original_ = nullptr;
    bool installed_ = false;
};

} // namespace vtablespp