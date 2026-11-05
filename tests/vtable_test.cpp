#include <gtest/gtest.h>

#include <optional>
#include <utility>
#include <array>
#include <iomanip>
#include <sstream>
#include <array>
#include <cstring>
#include <optional>
#include <sstream>
#include <iomanip>
#include <string>

#include <vtablespp/vtablespp.hpp>
#include <vtablespp/core/slot_classifier.hpp>

#include "fixtures.hpp"

namespace {

using namespace vtablespp_test;

using vtablespp::slot_classifier;
using vtablespp::slot_info;
using vtablespp::slot_kind;
using vtablespp::vtable_view;

/// Synthetic x64 thunk:
///
///   sub rcx, 8
///   jmp rel32
///
/// This is the canonical small this-adjustor pattern:
/// the thunk receives the secondary-base form of `this`
/// in rcx, subtracts the base-subobject offset, and jumps
/// to the shared final overrider.
constexpr unsigned char kSmallThisAdjustor[] =
    {0x48, 0x83, 0xE9, 0x08, 0xE9, 0x10, 0x00, 0x00, 0x00};

/// Synthetic x64 thunk with a 32-bit adjustment:
///
///   sub rcx, 0x12345678
///   jmp rel32
///
/// MSVC uses this form when the base-subobject offset
/// does not fit into a signed 8-bit immediate.
constexpr unsigned char kLargeThisAdjustor[] =
    {0x48, 0x81, 0xE9, 0x78, 0x56, 0x34, 0x12, 0xE9, 0x10, 0x00, 0x00, 0x00};

/// A plausible non-thunk function prologue.
///
/// It must not be classified as a this-adjustor merely
/// because it contains a `sub` instruction.
constexpr unsigned char kOrdinaryFunction[] = {
    0x48,
    0x89,
    0x4C,
    0x24,
    0x08, // mov [rsp+8], rcx
    0x48,
    0x83,
    0xEC,
    0x28 // sub rsp, 0x28
};

[[nodiscard]]
std::string to_hex(const unsigned char* bytes, std::size_t size) {
    std::ostringstream text;

    for (std::size_t i = 0; i < size; ++i) {
        if (i != 0) {
            text << ' ';
        }

        text << std::hex << std::setw(2) << std::setfill('0')
             << static_cast<int>(bytes[i]);
    }

    return text.str();
}

[[nodiscard]]
std::optional<const void*> resolve_jmp_rel32(const void* entry) {
    constexpr std::size_t kJmpRel32Size = 5;

    std::array<unsigned char, kJmpRel32Size> bytes{};

    if (!vtablespp::abi::read_code(entry, bytes.size(), bytes.data())) {
        return std::nullopt;
    }

    if (bytes[0] != 0xE9) {
        return std::nullopt;
    }

    std::int32_t displacement = 0;
    std::memcpy(&displacement, bytes.data() + 1, sizeof(displacement));

    const auto* base = static_cast<const unsigned char*>(entry);

    return base + kJmpRel32Size + displacement;
}

TEST(VTable, PolymorphicObjectHasNonZeroVTable) {
    SingleDerived object;

    // A polymorphic object must have a vfptr, so its primary
    // vftable address cannot be null.
    EXPECT_NE(vtablespp::vtable_of(object), nullptr);
}

TEST(VTable, SingleInheritancePrimaryTableIsAccessible) {
    SingleDerived object;

    // The caller supplies the size because reliable vftable-size
    // discovery is a separate, heuristic feature.
    const auto view = vtable_view::from_object(object, 3);

    EXPECT_FALSE(view.empty());
    EXPECT_EQ(view.size(), 3u);
}

TEST(VTable, MultipleInheritanceHasSeparatePrimaryAndSecondaryTables) {
    MultiDerived object;

    const void* primary = vtablespp::vtable_of(object);

    // Use the typed base-subobject view instead of manually adding
    // sizeof(void*) to the object address. The compiler knows the
    // actual offset of the Right subobject.
    const auto secondary_view = vtable_view::from_base<Right>(object, 1);

    const void* secondary =
        static_cast<const void*>(secondary_view.entries().data());

    EXPECT_NE(primary, nullptr);
    EXPECT_NE(secondary, nullptr);
}

TEST(VTableView, DefaultViewIsEmpty) {
    const vtable_view view;

    EXPECT_TRUE(view.empty());
    EXPECT_EQ(view.size(), 0u);
    EXPECT_TRUE(view.entries().empty());

    // `at()` returns optional: nullopt means "outside the supplied
    // range", rather than overloading nullptr with another meaning.
    EXPECT_EQ(view.at(0), std::nullopt);
}

TEST(VTableView, ExposesSuppliedEntries) {
    int first = 0;
    int second = 0;

    const vtable_view::slot_entry entries[] = {&first, &second};

    const vtable_view view{entries, 2};

    EXPECT_FALSE(view.empty());
    EXPECT_EQ(view.size(), 2u);

    ASSERT_TRUE(view.at(0).has_value());
    ASSERT_TRUE(view.at(1).has_value());

    EXPECT_EQ(*view.at(0), &first);
    EXPECT_EQ(*view.at(1), &second);

    EXPECT_EQ(view.at(2), std::nullopt);

    EXPECT_EQ(view.entries().data(), entries);
    EXPECT_EQ(view.entries().size(), 2u);
}

TEST(VTable, MultipleInheritanceExposesBothBaseTables) {
    MultiDerived object;

    const Left& left = object;
    const Right& right = object;

    // `Left` is the primary base, so from_object() is enough.
    const auto primary = vtable_view::from_object(left, 1);

    // `Right` is a secondary base. from_base<Right>() asks the
    // compiler to adjust to the Right subobject before reading
    // its vfptr.
    const auto secondary = vtable_view::from_base<Right>(object, 1);

    ASSERT_NE(primary.entries().data(), nullptr);
    ASSERT_NE(secondary.entries().data(), nullptr);

    EXPECT_NE(primary.entries().data(), secondary.entries().data());

    ASSERT_TRUE(primary.at(0).has_value());
    ASSERT_TRUE(secondary.at(0).has_value());

    EXPECT_NE(*primary.at(0), nullptr);
    EXPECT_NE(*secondary.at(0), nullptr);

    EXPECT_EQ(primary.at(1), std::nullopt);
    EXPECT_EQ(secondary.at(1), std::nullopt);
}

TEST(SlotClassifier, SmallThisAdjustorIsRecognized) {
    const vtable_view::slot_entry entries[] = {kSmallThisAdjustor};

    const vtable_view view{entries, 1};
    const slot_classifier classifier{view};

    const slot_info info = classifier.inspect(0);

    EXPECT_EQ(info.kind, slot_kind::this_adjustor);
    EXPECT_EQ(info.this_adjustment, 8);
    ASSERT_TRUE(info.final_target.has_value());
    EXPECT_NE(*info.final_target, nullptr);
}

TEST(SlotClassifier, LargeThisAdjustorIsRecognized) {
    const vtable_view::slot_entry entries[] = {kLargeThisAdjustor};

    const vtable_view view{entries, 1};
    const slot_classifier classifier{view};

    const slot_info info = classifier.inspect(0);

    EXPECT_EQ(info.kind, slot_kind::this_adjustor);
    EXPECT_EQ(info.this_adjustment, 0x12345678);
    ASSERT_TRUE(info.final_target.has_value());
}

TEST(SlotClassifier, OrdinaryFunctionIsNotThisAdjustor) {
    const vtable_view::slot_entry entries[] = {kOrdinaryFunction};

    const vtable_view view{entries, 1};
    const slot_classifier classifier{view};

    const slot_info info = classifier.inspect(0);

    // The classifier is conservative: if the exact
    // `sub rcx, N; jmp` pattern is absent, it must not guess.
    EXPECT_EQ(info.kind, slot_kind::unknown);
    EXPECT_EQ(info.this_adjustment, 0);
    EXPECT_EQ(info.final_target, std::nullopt);
}

TEST(SlotClassifier, OutOfRangeSlotIsUnknown) {
    const vtable_view::slot_entry entries[] = {kSmallThisAdjustor};

    const vtable_view view{entries, 1};
    const slot_classifier classifier{view};

    const slot_info info = classifier.inspect(1);

    EXPECT_EQ(info.kind, slot_kind::unknown);
    EXPECT_EQ(info.entry, nullptr);
    EXPECT_EQ(info.final_target, std::nullopt);
}

TEST(SlotClassifier, SeparateBaseImplementationsHaveNoThisAdjustor) {
    SeparateImplementationsDerived object;

    const auto secondary =
        vtable_view::from_base<SeparateRight>(object, 1);

    const slot_classifier classifier{secondary};

    const slot_info info = classifier.inspect(0);

    // Each base keeps its own implementation, so MSVC does not
    // need a this-adjustor thunk in the Right vftable.
    //
    // Until direct-entry classification is implemented, the
    // conservative result is `unknown`, not `direct`.
    EXPECT_EQ(info.kind, slot_kind::unknown);
    EXPECT_EQ(info.this_adjustment, 0);
    EXPECT_EQ(info.final_target, std::nullopt);
}

TEST(SlotClassifier, SharedOverrideCreatesSecondaryThisAdjustor) {
    SharedOverrideDerived object;

    const auto secondary =
        vtable_view::from_base<SharedRight>(object, 1);

    const slot_classifier classifier{secondary};

    const slot_info info = classifier.inspect(0);

    EXPECT_EQ(info.kind, slot_kind::this_adjustor);
    EXPECT_EQ(info.this_adjustment, sizeof(void*));
    ASSERT_TRUE(info.final_target.has_value());
}

} // namespace