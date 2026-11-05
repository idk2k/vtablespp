#include "vtablespp/core/slot_classifier.hpp"

#include <array>
#include <cstring>

#include "vtablespp/abi/abi.hpp"

namespace vtablespp {
namespace {

[[nodiscard]]
std::optional<const void*> resolve_jmp_rel32(
    const void* entry
) noexcept {
    constexpr std::size_t kJmpRel32Size = 5;

    unsigned char bytes[5]{};

    if (!abi::read_code(entry, sizeof(bytes), bytes)) {
        return std::nullopt;
    }

    if (bytes[0] != 0xE9) {
        return std::nullopt;
    }

    std::int32_t displacement = 0;
    std::memcpy(&displacement, bytes + 1, sizeof(displacement));

    const auto* base =
        static_cast<const unsigned char*>(entry);

    return base + kJmpRel32Size + displacement;
}

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
struct thunk_match {
    bool matched = false;
    std::ptrdiff_t adjustment = 0;
    const void* target = nullptr;
};

[[nodiscard]]
thunk_match match_this_adjustor(const void* entry) noexcept {
    thunk_match result;

    constexpr std::size_t kBytes = 16;
    std::array<std::byte, kBytes> buffer{};

    if (!vtablespp::abi::read_code(
            entry,
            buffer.size(),
            buffer.data()
        )) {
        return result;
    }

    const auto* p =
        reinterpret_cast<const unsigned char*>(buffer.data());

    std::size_t prefix_size = 0;
    std::ptrdiff_t adjustment = 0;

    if (p[0] == 0x48 && p[1] == 0x83 && p[2] == 0xE9) {
        prefix_size = 4;
        adjustment = static_cast<std::int8_t>(p[3]);
    } else if (p[0] == 0x48 && p[1] == 0x81 && p[2] == 0xE9) {
        prefix_size = 7;

        std::int32_t raw = 0;
        std::memcpy(&raw, p + 3, sizeof(raw));
        adjustment = raw;
    } else {
        return result;
    }

    const unsigned char* jump = p + prefix_size;
    const auto* base =
        static_cast<const unsigned char*>(entry);

    if (jump[0] == 0xE9) {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, jump + 1, sizeof(displacement));

        result.matched = true;
        result.adjustment = adjustment;
        result.target =
            base + prefix_size + 5 + displacement;
        return result;
    }

    if (jump[0] == 0xFF && jump[1] == 0x25) {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, jump + 2, sizeof(displacement));

        const void* slot =
            base + prefix_size + 6 + displacement;

        const void* target = nullptr;
        if (!vtablespp::abi::read_code(
                slot,
                sizeof(target),
                &target
            )) {
            return result;
        }

        result.matched = true;
        result.adjustment = adjustment;
        result.target = target;
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

    // First, try the entry itself.
    auto adjustor = match_this_adjustor(entry);

    // If this is an incremental-linking jump stub,
    // follow exactly one jump and inspect its target.
    if (!adjustor.matched) {
        if (const auto resolved = resolve_jmp_rel32(entry)) {
            adjustor = match_this_adjustor(*resolved);

            if (adjustor.matched) {
                info.final_target = adjustor.target;
            }
        }
    } else {
        info.final_target = adjustor.target;
    }

    if (adjustor.matched) {
        info.kind = slot_kind::this_adjustor;
        info.this_adjustment = adjustor.adjustment;
    }

    return info;
}

} // namespace vtablespp