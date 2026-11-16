#pragma once

#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>
#include <set>
#include <tuple>

#include <windows.h>

#include "vtablespp/abi/abi.hpp"
#include "vtablespp/core/call_slot.hpp"
#include "vtablespp/core/slot_hook.hpp"
#include "vtablespp/core/slot_info.hpp"
#include "vtablespp/core/vtable.hpp"

namespace vtablespp {

template <class Signature>
class slot_context;

template <class Signature>
class stateful_slot_hook;

template <class Return, class... Args>
class slot_context<Return(Args...)> {
public:
    using function_type = Return (*)(void*, Args...);

    slot_context(void* entry, void* object) noexcept
        : entry_(entry), object_(object) {}

    Return operator()(Args... args) {
        auto* function = reinterpret_cast<function_type>(entry_);

        return function(object_, std::forward<Args>(args)...);
    }

    Return call_original_raw(void* object, Args... args) {
        auto* function = reinterpret_cast<function_type>(entry_);

        return function(object, std::forward<Args>(args)...);
    }

    void* original_entry() const noexcept {
        return entry_;
    }

    void* entry() const noexcept {
        return entry_;
    }

private:
    void* entry_;
    void* object_;
};

template <class Return, class... Args>
class stateful_slot_hook<Return(Args...)> {
public:
    using signature_type = Return(Args...);

    using context_type = slot_context<Return(Args...)>;

    stateful_slot_hook() = default;

    ~stateful_slot_hook() {
        uninstall();
    }

    stateful_slot_hook(const stateful_slot_hook&) = delete;

    stateful_slot_hook& operator=(const stateful_slot_hook&) = delete;

    stateful_slot_hook(stateful_slot_hook&& other) noexcept
        : storage_(std::move(other.storage_)), table_(other.table_),
          slot_(other.slot_), object_(other.object_),
          installed_(other.installed_.load()) {
        other.installed_.store(false);
    }

    stateful_slot_hook& operator=(stateful_slot_hook&& other) noexcept {
        if (this != &other) {
            uninstall();

            storage_ = std::move(other.storage_);
            table_ = other.table_;
            slot_ = other.slot_;
            object_ = other.object_;
            installed_.store(other.installed_.load());

            other.installed_.store(false);
        }

        return *this;
    }

    template <class Object, class Callback>
    bool install(
        const vtable_view& table,
        std::size_t slot,
        Object& object,
        Callback&& callback
    ) {
        return install_impl(
            table,
            slot,
            static_cast<void*>(&object),
            std::make_unique<callable_impl<std::decay_t<Callback>>>(
                std::forward<Callback>(callback)
            )
        );
    }

    void uninstall() {
        if (!installed_.exchange(false)) {
            return;
        }

        if (!storage_) {
            return;
        }

        const auto slot_key =
            std::make_tuple(table_.vtable_address(), slot_, object_);

        {
            std::scoped_lock lock(registry_mutex);

            installed_slots.erase(slot_key);
        }

        const void* original = storage_->original_entry;

        abi::write_memory(
            table_.vtable_address() + slot_,
            sizeof(const void*),
            &original
        );

        free_trampoline(storage_->trampoline);

        storage_.reset();
    }

    bool is_installed() const noexcept {
        return installed_.load();
    }

private:
    struct callable_base {
        virtual ~callable_base() = default;

        virtual Return call(context_type&, void*, Args...) = 0;
    };

    template <class F>
    struct callable_impl final : callable_base {
        F callback;

        explicit callable_impl(F&& f) : callback(std::move(f)) {}

        Return
        call(context_type& context, void* object, Args... args) override {
            return callback(context, object, std::forward<Args>(args)...);
        }
    };

    struct context_storage {
        void* original_entry = nullptr;
        void* trampoline = nullptr;
        std::unique_ptr<callable_base> hook;
    };

    bool install_impl(
        const vtable_view& table,
        std::size_t slot,
        void* object,
        std::unique_ptr<callable_base> callback
    ) {
        if (installed_.load()) {
            return false;
        }

        const auto entry = table.at(slot);

        if (!entry.has_value() || *entry == nullptr) {
            return false;
        }

        const auto purecall = abi::purecall_address();

        if (purecall.has_value() && *entry == *purecall) {
            return false;
        }

        const auto slot_key =
            std::make_tuple(table.vtable_address(), slot, object);

        {
            std::scoped_lock lock(registry_mutex);

            if (installed_slots.count(slot_key) != 0) {
                return false;
            }
        }

        auto storage = std::make_shared<context_storage>();

        storage->original_entry = const_cast<void*>(*entry);
        storage->hook = std::move(callback);

        void* trampoline = allocate_trampoline(
            storage.get(),
            reinterpret_cast<const void*>(&invoke)
        );

        if (trampoline == nullptr) {
            return false;
        }

        storage->trampoline = trampoline;

        const void* replacement = trampoline;

        const bool written = abi::write_memory(
            table.vtable_address() + slot,
            sizeof(const void*),
            &replacement
        );

        std::printf(
            "[hook] install: written=%d slot_now=%p\n",
            static_cast<int>(written),
            table.at(slot).value_or(nullptr)
        );
        std::fflush(stdout);

        if (!written) {
            free_trampoline(trampoline);
            return false;
        }

        {
            std::scoped_lock lock(registry_mutex);

            installed_slots.insert(slot_key);
        }

        storage_ = std::move(storage);
        table_ = table;
        slot_ = slot;
        object_ = object;

        installed_.store(true);

        return true;
    }

    static void*
    allocate_trampoline(context_storage* context, const void* dispatcher) {
        constexpr std::size_t kTrampolineSize = 32;

        void* memory = VirtualAlloc(
            nullptr,
            kTrampolineSize,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE
        );

        if (memory == nullptr) {
            return nullptr;
        }

        auto* code = static_cast<unsigned char*>(memory);

        // mov rax, context
        code[0] = 0x48;
        code[1] = 0xB8;

        std::memcpy(code + 2, &context, sizeof(context));

        // mov r11, dispatcher
        code[10] = 0x49;
        code[11] = 0xBB;

        std::memcpy(code + 12, &dispatcher, sizeof(dispatcher));

        // sub rsp, 0x28  (40 байт: home space + запас, чтобы пролог
        //                  invoke не затер RA1)
        code[20] = 0x48;
        code[21] = 0x83;
        code[22] = 0xEC;
        code[23] = 0x28;

        // call r11
        code[24] = 0x41;
        code[25] = 0xFF;
        code[26] = 0xD3;

        // add rsp, 0x28
        code[27] = 0x48;
        code[28] = 0x83;
        code[29] = 0xC4;
        code[30] = 0x28;

        // ret
        code[31] = 0xC3;

        DWORD old_protect = 0;

        if (!VirtualProtect(
                memory,
                kTrampolineSize,
                PAGE_EXECUTE_READ,
                &old_protect
            )) {
            VirtualFree(memory, 0, MEM_RELEASE);
            return nullptr;
        }

        FlushInstructionCache(GetCurrentProcess(), memory, kTrampolineSize);

        return memory;
    }

    static void free_trampoline(void* trampoline) {
        if (trampoline != nullptr) {
            VirtualFree(trampoline, 0, MEM_RELEASE);
        }
    }

    static Return invoke(void* object, Args... args) {
        void** return_address_slot =
            reinterpret_cast<void**>(_AddressOfReturnAddress());

        const auto* return_address =
            static_cast<const unsigned char*>(*return_address_slot);

        void* raw_context = nullptr;

        std::memcpy(&raw_context, return_address - 25, sizeof(raw_context));

        auto* context = static_cast<context_storage*>(raw_context);

        context_type context_view(context->original_entry, object);

        Return result = context->hook->call(
            context_view,
            object,
            std::forward<Args>(args)...
        );

        std::printf("[hook] invoke returned from hook\n");
        std::fflush(stdout);

        return result;
    }

    std::shared_ptr<context_storage> storage_;
    vtable_view table_;
    std::size_t slot_ = 0;
    void* object_ = nullptr;
    std::atomic<bool> installed_{false};

    static std::mutex registry_mutex;
    static std::set<std::tuple<const void*, std::size_t, void*>>
        installed_slots;
};

template <class Return, class... Args>
std::mutex stateful_slot_hook<Return(Args...)>::registry_mutex;

template <class Return, class... Args>
std::set<std::tuple<const void*, std::size_t, void*>>
    stateful_slot_hook<Return(Args...)>::installed_slots;

} // namespace vtablespp