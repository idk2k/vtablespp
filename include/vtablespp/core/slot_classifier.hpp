#pragma once

#include "vtable.hpp"
#include "slot_info.hpp"

namespace vtablespp {

/// Classifies individual vftable entries without calling them.
///
/// This is intentionally conservative:
/// - an unrecognized entry is `slot_kind::unknown`;
/// - return-adjustor detection is not enabled yet;
/// - classification never writes memory and never invokes an entry.
class slot_classifier {
public:
    explicit slot_classifier(const vtable_view& table) noexcept
        : table_(table) {}

    [[nodiscard]]
    slot_info inspect(std::size_t index) const noexcept;

private:
    [[nodiscard]]
    slot_info classify_entry(const void* entry) const noexcept;

    vtable_view table_;
};
} // namespace vtablespp