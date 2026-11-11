#include "vtablespp/core/slot_classifier.hpp"

#include <array>
#include <cstring>

#include "vtablespp/abi/abi.hpp"

namespace vtablespp {
namespace {

namespace {
[[nodiscard]]
bool is_readable_code(const void* entry) noexcept {
    unsigned char first_byte = 0;
    return abi::read_code(entry, sizeof(first_byte), &first_byte);
}
} // namespace

[[nodiscard]]
std::optional<const void*> resolve_jmp_rel32(const void* entry) noexcept {
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

    const auto* base = static_cast<const unsigned char*>(entry);

    return base + kJmpRel32Size + displacement;
}

[[nodiscard]]
bool is_purecall(const void* entry) noexcept {
    const std::optional<const void*> purecall = abi::purecall_address();

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

    if (!vtablespp::abi::read_code(entry, buffer.size(), buffer.data())) {
        return result;
    }

    const auto* p = reinterpret_cast<const unsigned char*>(buffer.data());

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
    const auto* base = static_cast<const unsigned char*>(entry);

    if (jump[0] == 0xE9) {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, jump + 1, sizeof(displacement));

        result.matched = true;
        result.adjustment = adjustment;
        result.target = base + prefix_size + 5 + displacement;
        return result;
    }

    if (jump[0] == 0xFF && jump[1] == 0x25) {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, jump + 2, sizeof(displacement));

        const void* slot = base + prefix_size + 6 + displacement;

        const void* target = nullptr;
        if (!vtablespp::abi::read_code(slot, sizeof(target), &target)) {
            return result;
        }

        result.matched = true;
        result.adjustment = adjustment;
        result.target = target;
        return result;
    }

    return result;
}

struct return_thunk_match {
    bool matched = false;
    std::ptrdiff_t adjustment = 0;
    bool is_virtual = false;
    const void* target = nullptr; // target function whicn thunk calls
};

[[nodiscard]]
return_thunk_match match_return_adjustor(const void* entry) noexcept {
    return_thunk_match result;

    constexpr std::size_t kBytes = 64;
    std::array<std::byte, kBytes> buffer{};

    if (!vtablespp::abi::read_code(entry, buffer.size(), buffer.data())) {
        return result;
    }

    const auto* p = reinterpret_cast<const unsigned char*>(buffer.data());
    const auto* base = static_cast<const unsigned char*>(entry);

    // 1. Critical fix: for ordinal functions exclusion from check:
    // Actual return thunk should contain call at the beginning.
    bool has_call_instruction = false;
    std::size_t call_pos = 0;
    bool is_direct_call = false;

    for (std::size_t i = 0; i < 24; ++i) {
        if (p[i] == 0xE8) {
            has_call_instruction = true;
            call_pos = i;
            is_direct_call = true;
            break;
        }
        if (p[i] == 0xFF) {
            has_call_instruction = true;
            call_pos = i;
            is_direct_call = false;
            break;
        }
    }

    if (!has_call_instruction) {
        return result; // its ordinal function, then ignore it
    }

    // calculate base target, if call is direct (E8)
    const void* computed_target = entry;
    if (is_direct_call) {
        std::int32_t call_displacement = 0;
        std::memcpy(
            &call_displacement,
            p + call_pos + 1,
            sizeof(call_displacement)
        );
        computed_target = base + call_pos + 5 + call_displacement;

        // allow INCEMENTAL-flag jump stub, if it exists (/INCREMENTAL on MSVC)
        if (const auto resolved_target = resolve_jmp_rel32(computed_target)) {
            computed_target = *resolved_target;
        }
    }

    // lambda for detect adjustment instruction for RAX
    auto check_adjustment = [&](std::size_t pos) -> bool {
        if (pos + 3 >= kBytes)
            return false;

        // add rax, imm8 (48 83 C0 ib)
        if (p[pos] == 0x48 && p[pos + 1] == 0x83 && p[pos + 2] == 0xC0) {
            result.matched = true;
            result.adjustment = static_cast<std::int8_t>(p[pos + 3]);
            result.target = computed_target;
            return true;
        }
        // lea rax, [rax+imm8] (48 8D 40 ib)
        if (p[pos] == 0x48 && p[pos + 1] == 0x8D && p[pos + 2] == 0x40) {
            result.matched = true;
            result.adjustment = static_cast<std::int8_t>(p[pos + 3]);
            result.target = computed_target;
            return true;
        }

        if (pos + 6 >= kBytes)
            return false;

        // add rax, imm32 (48 05 id id id id)
        if (p[pos] == 0x48 && p[pos + 1] == 0x05) {
            std::int32_t imm32 = 0;
            std::memcpy(&imm32, p + pos + 2, sizeof(imm32));
            result.matched = true;
            result.adjustment = imm32;
            result.target = computed_target;
            return true;
        }
        // add rax, imm32 alt (48 81 C0 id id id id)
        if (p[pos] == 0x48 && p[pos + 1] == 0x81 && p[pos + 2] == 0xC0) {
            std::int32_t imm32 = 0;
            std::memcpy(&imm32, p + pos + 3, sizeof(imm32));
            result.matched = true;
            result.adjustment = imm32;
            result.target = computed_target;
            return true;
        }
        // lea rax, [rax+imm32] (48 8D 80 id id id id)
        if (p[pos] == 0x48 && p[pos + 1] == 0x8D && p[pos + 2] == 0x80) {
            std::int32_t imm32 = 0;
            std::memcpy(&imm32, p + pos + 3, sizeof(imm32));
            result.matched = true;
            result.adjustment = imm32;
            result.target = computed_target;
            return true;
        }

        // Virtual inheritance vbtable lookup
        if (pos + 6 < kBytes && p[pos] == 0x48 && p[pos + 1] == 0x8B &&
            p[pos + 2] == 0x00 && p[pos + 3] == 0x48 && p[pos + 4] == 0x63 &&
            p[pos + 5] == 0x40 && p[pos + 6] == 0x04) {
            result.matched = true;
            result.is_virtual = true;
            result.adjustment = 0;
            result.target = computed_target;
            return true;
        }

        return false;
    };

    auto is_clean_gap = [&](std::size_t start, std::size_t end) -> bool {
        for (std::size_t i = start; i < end && i < kBytes; ++i) {
            unsigned char b = p[i];
            if (b == 0xE8 || b == 0xE9 || b == 0xC3 || b == 0xC2 || b == 0xEB)
                return false;
            if (b == 0x0F && i + 1 < kBytes && (p[i + 1] & 0xF0) == 0x80)
                return false;
            if ((b & 0xF0) == 0x70)
                return false;
            if (b == 0xFF && i + 1 < kBytes &&
                ((p[i + 1] & 0x38) == 0x10 || (p[i + 1] & 0x38) == 0x20))
                return false;
        }
        return true;
    };

    // Try to find conditional jump ONLY after detection call instruction
    for (std::size_t j = call_pos; j < kBytes - 8; ++j) {
        bool is_je_short = (p[j] == 0x74);
        bool is_je_near = (p[j] == 0x0F && p[j + 1] == 0x84);
        bool is_jnz_short = (p[j] == 0x75);
        bool is_jnz_near = (p[j] == 0x0F && p[j + 1] == 0x85);

        if (!is_je_short && !is_je_near && !is_jnz_short && !is_jnz_near)
            continue;

        if (is_je_short || is_je_near) {
            std::size_t start_pos = j + (is_je_short ? 2 : 6);
            constexpr std::size_t kMaxWindow = 14;
            std::size_t window_end = std::min(start_pos + kMaxWindow, kBytes);

            for (std::size_t k = start_pos; k < window_end; ++k) {
                if (!is_clean_gap(start_pos, k)) {
                    break;
                }
                if (check_adjustment(k))
                    return result;
            }
        }

        if (is_jnz_short) {
            std::int8_t disp = static_cast<std::int8_t>(p[j + 1]);
            std::size_t target_pos = j + 2 + disp;
            if (target_pos < kBytes && check_adjustment(target_pos))
                return result;
        }
        if (is_jnz_near) {
            std::int32_t disp = 0;
            std::memcpy(&disp, p + j + 2, sizeof(disp));
            std::size_t target_pos = j + 6 + disp;
            if (target_pos < kBytes && check_adjustment(target_pos))
                return result;
        }
    }

    return result;
}

} // namespace

// slot_info slot_classifier::inspect(std::size_t index) const noexcept {
//     const std::optional<vtable_view::slot_entry> entry = table_.at(index);

//     if (!entry.has_value()) {
//         return slot_info{};
//     }

//     return classify_entry(*entry);
// }

slot_info slot_classifier::classify_entry(const void* entry) const noexcept {
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
        info.final_target = adjustor.target;

        resolve_symbol(info);

        return info;
    }

    auto ret_adjustor = match_return_adjustor(entry);

    if (!ret_adjustor.matched) {
        if (const auto resolved = resolve_jmp_rel32(entry)) {
            ret_adjustor = match_return_adjustor(*resolved);

            if (ret_adjustor.matched) {
                info.final_target = ret_adjustor.target;
            }
        }
    } else {
        info.final_target = ret_adjustor.target;
    }

    if (ret_adjustor.matched) {
        info.kind = slot_kind::return_adjustor;
        info.return_adjustment = ret_adjustor.adjustment;
        info.is_virtual_thunk = ret_adjustor.is_virtual;
        info.final_target = ret_adjustor.target;

        resolve_symbol(info);

        return info;
    }

    // 3. Not a pure-call and not a recognized thunk:
    // treat it as a direct virtual-function entry.
    if (is_readable_code(entry)) {
        info.kind = slot_kind::direct;
        info.evidence = direct_evidence::none;

        resolve_symbol(info);

        return info;
    }

    info.kind = slot_kind::unknown;
    return info;
}

void slot_classifier::resolve_symbol(slot_info& info) const noexcept {
    if (!resolver_) {
        return;
    }

    const void* address = info.final_target.value_or(info.entry);

    if (address == nullptr) {
        return;
    }

    auto symbol = resolver_->resolve(address);

    if (!symbol) {
        return;
    }

    info.symbol_name = std::move(symbol->name);
    info.demangled_name = std::move(symbol->demangled_name);
    info.symbol_displacement = symbol->displacement;
    info.module = std::move(symbol->module);

    if (info.kind == slot_kind::direct) {
        info.evidence = direct_evidence::symbol_resolved;
    }
}

void slot_classifier::resolve_rtti(slot_info& info) const noexcept {
    if (!rtti_) {
        return;
    }

    const rtti::rtti_reader reader{table_.vtable_address()};
    const auto metadata = reader.read();

    if (!metadata.has_value()) {
        return;
    }

    info.has_rtti = true;
    info.rtti_class_name = metadata->class_name;
    info.rtti_vtable_offset = metadata->vtable_offset;
}

slot_info slot_classifier::inspect(std::size_t index) const noexcept {
    const auto entry = table_.at(index);

    slot_info info = entry.has_value()
        ? classify_entry(*entry)
        : slot_info{};

    resolve_rtti(info);

    return info;
}

} // namespace vtablespp