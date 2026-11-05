#include "vtablespp/core/slot_classifier.hpp"

#include <array>
#include <cstring>

#include "vtablespp/abi/abi.hpp"

namespace vtablespp {
namespace {

struct this_adjustor_result {
    bool matched = false;
    std::ptrdiff_t adjustment = 0;
    const void* jump_target = nullptr;
};

[[nodiscard]]
bool is_purecall(const void* entry) noexcept {
    const std::optional<const void*> purecall =
        abi::purecall_address();

    return purecall.has_value() && entry == *purecall;
}

/// Recognizes:
///   48 83 E9 ib      sub rcx, imm8
///   48 81 E9 id      sub rcx, imm32
/// followed by one of:
///   E9 rel32         jmp rel32
///   FF 25 rel32      jmp qword ptr [rip + rel32]
[[nodiscard]]
this_adjustor_result match_this_adjustor(
    const void* entry
) noexcept {
    this_adjustor_result result;

    constexpr std::size_t max_prefix = 7;
    std::array<std::byte, max_prefix> bytes{};

    if (!abi::read_code(entry, bytes.size(), bytes.data())) {
        return result;
    }

    const auto* p = reinterpret_cast<const unsigned char*>(bytes.data());
    std::size_t adjustment_size = 0;
    std::ptrdiff_t adjustment = 0;

    if (p[0] == 0x48 && p[1] == 0x83 && p[2] == 0xE9) {
        adjustment_size = 4;
        adjustment = static_cast<std::int8_t>(p[3]);
    } else if (p[0] == 0x48 && p[1] == 0x81 && p[2] == 0xE9) {
        adjustment_size = 7;

        std::int32_t raw = 0;
        std::memcpy(&raw, p + 3, sizeof(raw));
        adjustment = raw;
    } else {
        return result;
    }

    const unsigned char* jump = p + adjustment_size;

    if (jump[0] == 0xE9) {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, jump + 1, sizeof(displacement));

        const auto* entry_bytes =
            static_cast<const unsigned char*>(entry);

        result.matched = true;
        result.adjustment = adjustment;
        result.jump_target =
            entry_bytes + adjustment_size + 5 + displacement;
        return result;
    }

    if (jump[0] == 0xFF && jump[1] == 0x25) {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, jump + 2, sizeof(displacement));

        const auto* entry_bytes =
            static_cast<const unsigned char*>(entry);

        const void* indirect_slot =
            entry_bytes + adjustment_size + 6 + displacement;

        // The target is stored at the RIP-relative location.
        // Read it through the same safe-read interface.
        const void* target = nullptr;
        if (!abi::read_code(
                indirect_slot,
                sizeof(target),
                &target
            )) {
            return result;
        }

        result.matched = true;
        result.adjustment = adjustment;
        result.jump_target = target;
        return result;
    }

    return result;
}

} // namespace

slot_info slot_classifier::inspect(
    std::size_t index
) const noexcept {
    const std::optional<vtable_view::slot_entry> entry =
        table_.at(index);

    if (!entry.has_value()) {
        return slot_info{};
    }

    return classify_entry(*entry);
}

slot_info slot_classifier::classify_entry(
    const void* entry
) const noexcept {
    slot_info info;
    info.entry = entry;
    info.kind = slot_kind::unknown;

    if (entry == nullptr) {
        return info;
    }

    if (is_purecall(entry)) {
        info.kind = slot_kind::pure_call;
        return info;
    }

    const this_adjustor_result adjustor =
        match_this_adjustor(entry);

    if (adjustor.matched) {
        info.kind = slot_kind::this_adjustor;
        info.this_adjustment = adjustor.adjustment;
        info.final_target = adjustor.jump_target;
    }

    return info;
}

} // namespace vtablespp