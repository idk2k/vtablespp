#pragma once

#include <cstddef>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "vtablespp/abi/abi.hpp"

#if defined(_WIN32) && defined(_WIN64)
#include <Windows.h>
#define VTABLESPP_VTABLE_OVERRIDE_SUPPORTED 1
#else
#define VTABLESPP_VTABLE_OVERRIDE_SUPPORTED 0
#endif

namespace vtablespp {
namespace detail {

class vtable_override {
public:
    vtable_override(
        void* object,
        const void* current_vtable,
        std::size_t slot_count
    )
        : object_(object), previous_vtable_(current_vtable),
          slot_count_(slot_count), hook_count_(0), vptr_switched_(false) {
        const std::size_t bytes = (slot_count + 1) * sizeof(const void*);

        cloned_vtable_ = static_cast<unsigned char*>(VirtualAlloc(
            nullptr,
            bytes,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE
        ));

        if (cloned_vtable_ != nullptr) {
            std::memcpy(
                cloned_vtable_,
                static_cast<const unsigned char*>(current_vtable) -
                    sizeof(const void*),
                bytes
            );

            cloned_entries_ = reinterpret_cast<const void**>(
                cloned_vtable_ + sizeof(const void*)
            );
        }
    }

    ~vtable_override() {
        if (cloned_vtable_ == nullptr) {
            return;
        }

        if (vptr_switched_) {
            const void* restored = previous_vtable_;

            abi::write_memory(object_, sizeof(const void*), &restored);
        }

        VirtualFree(cloned_vtable_, 0, MEM_RELEASE);

        cloned_vtable_ = nullptr;
        cloned_entries_ = nullptr;
    }

    [[nodiscard]]
    bool valid() const noexcept {
        return cloned_vtable_ != nullptr;
    }

    [[nodiscard]]
    bool has_hook(std::size_t index) const {
        return hooked_slots_.find(index) != hooked_slots_.end();
    }

    bool install_slot(
        std::size_t index,
        const void* trampoline,
        const void* original_entry
    ) {
        if (!valid() || index >= slot_count_) {
            return false;
        }

        if (has_hook(index)) {
            return false;
        }

        cloned_entries_[index] = trampoline;
        hooked_slots_.emplace(index, original_entry);

        if (hook_count_ == 0) {
            const void* new_vtable =
                reinterpret_cast<const void*>(cloned_entries_);

            if (!abi::write_memory(object_, sizeof(const void*), &new_vtable)) {
                cloned_entries_[index] = original_entry;
                hooked_slots_.erase(index);
                return false;
            }

            vptr_switched_ = true;
        }

        ++hook_count_;

        return true;
    }

    void uninstall_slot(std::size_t index) {
        const auto found = hooked_slots_.find(index);

        if (found == hooked_slots_.end()) {
            return;
        }

        cloned_entries_[index] = found->second;
        hooked_slots_.erase(found);
        --hook_count_;

        if (hook_count_ == 0 && vptr_switched_) {
            const void* restored = previous_vtable_;

            abi::write_memory(object_, sizeof(const void*), &restored);

            vptr_switched_ = false;
        }
    }

    [[nodiscard]]
    std::size_t hook_count() const noexcept {
        return hook_count_;
    }

    [[nodiscard]]
    void* object() const noexcept {
        return object_;
    }

    static std::shared_ptr<vtable_override>
    acquire(void* object, const void* current_vtable, std::size_t slot_count) {
        auto& registry = override_registry();

        const auto found = registry.find(object);

        if (found != registry.end()) {
            return found->second;
        }

        auto created = std::make_shared<vtable_override>(
            object,
            current_vtable,
            slot_count
        );

        if (!created->valid()) {
            return nullptr;
        }

        registry.emplace(object, created);

        return created;
    }

    static void release(void* object) {
        override_registry().erase(object);
    }

private:
    static std::unordered_map<void*, std::shared_ptr<vtable_override>>&
    override_registry() {
        static std::unordered_map<void*, std::shared_ptr<vtable_override>>
            registry;

        return registry;
    }

    void* object_;
    const void* previous_vtable_;
    unsigned char* cloned_vtable_ = nullptr;
    const void** cloned_entries_ = nullptr;
    std::size_t slot_count_ = 0;
    std::size_t hook_count_ = 0;
    bool vptr_switched_ = false;
    std::unordered_map<std::size_t, const void*> hooked_slots_;
};

inline std::mutex& override_mutex() noexcept {
    static std::mutex mutex;
    return mutex;
}

} // namespace detail
} // namespace vtablespp