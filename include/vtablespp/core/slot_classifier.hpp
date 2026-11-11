#pragma once

#include "vtable.hpp"
#include "slot_info.hpp"

#include <memory>
#include "vtablespp/symbols/symbol_resolver.hpp"

namespace vtablespp {

/// Classifies individual vftable entries without calling them.
///
/// This is intentionally conservative:
/// - an unrecognized entry is `slot_kind::unknown`;
/// - return-adjustor detection is not enabled yet;
/// - classification never writes memory and never invokes an entry.
class slot_classifier {
public:
    explicit slot_classifier(
        const vtable_view& table,
        std::shared_ptr<const symbols::symbol_resolver> resolver = nullptr
    ) noexcept
        : table_(table),
          resolver_(std::move(resolver)) {}

    [[nodiscard]]
    slot_info inspect(std::size_t index) const noexcept;

private:
    [[nodiscard]]
    slot_info classify_entry(const void* entry) const noexcept;

    [[nodiscard]]
    void resolve_symbol(slot_info& info) const noexcept;

    vtable_view table_;
    std::shared_ptr<const symbols::symbol_resolver> resolver_;
};
} // namespace vtablespp