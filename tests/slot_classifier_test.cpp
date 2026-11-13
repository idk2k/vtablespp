#include <gtest/gtest.h>

#include <array>

#include <vtablespp/vtablespp.hpp>
#include <vtablespp/core/slot_classifier.hpp>

namespace {

using vtablespp::slot_classifier;
using vtablespp::slot_info;
using vtablespp::slot_kind;
using vtablespp::vtable_view;

constexpr unsigned char kSmallThisAdjustor[] =
    {0x48, 0x83, 0xE9, 0x08, 0xE9, 0x10, 0x00, 0x00, 0x00};

constexpr unsigned char kLargeThisAdjustor[] =
    {0x48, 0x81, 0xE9, 0x78, 0x56, 0x34, 0x12, 0xE9, 0x10, 0x00, 0x00, 0x00};

constexpr unsigned char kOrdinaryFunction[] =
    {0x48, 0x89, 0x4C, 0x24, 0x08, 0x48, 0x83, 0xEC, 0x28};

// ============================================================================
// Synthetic Return Adjustor (Covariant) Patterns
// ============================================================================

// Classic MSVC Debug: prologs -> call -> cmp rax, 0 -> je -> add rax, 8 -> ret
constexpr unsigned char kMsvcDebugReturnAdjustor[] = {
    0x48, 0x89, 0x4C, 0x24,
    0x08,                   // mov [rsp+8], rcx
    0x48, 0x83, 0xEC, 0x38, // sub rsp, 38h
    0xE8, 0x00, 0x00, 0x00,
    0x00,                   // call +0 (points directly to next instruction)
    0x48, 0x83, 0xF8, 0x00, // cmp rax, 0
    0x74, 0x04,             // je +4
    0x48, 0x83, 0xC0, 0x08, // add rax, 8
    0x48, 0x83, 0xC4, 0x38, // add rsp, 38h
    0xC3                    // ret
};

// MSVC Release (/O2): call -> test rax, rax -> je -> add rax, 32-bit imm -> ret
constexpr unsigned char kMsvcReleaseLargeReturnAdjustor[] = {
    0xE8,
    0x00,
    0x00,
    0x00,
    0x00, // call +0
    0x48,
    0x85,
    0xC0, // test rax, rax
    0x74,
    0x07, // je +7
    0x48,
    0x05,
    0x78,
    0x56,
    0x34,
    0x12, // add rax, 0x12345678
    0xC3  // ret
};

// MSVC Virtual Inheritance Return Adjustor: call -> test rax, rax -> je -> mov
// rax, [rax] -> movsxd -> add
constexpr unsigned char kMsvcVirtualReturnAdjustor[] = {
    0xE8,
    0x00,
    0x00,
    0x00,
    0x00, // call +0
    0x48,
    0x85,
    0xC0, // test rax, rax
    0x74,
    0x0A, // je +10
    0x48,
    0x8B,
    0x00, // mov rax, [rax]
    0x48,
    0x63,
    0x40,
    0x04, // movsxd rax, dword ptr [rax+4]
    0xC3  // ret
};

TEST(SlotClassifier, SmallThisAdjustorIsRecognized) {
    const vtable_view::slot_entry entries[] = {kSmallThisAdjustor};
    const vtable_view view{entries, 1};
    const slot_classifier classifier{view};
    const slot_info info = classifier.inspect(0);

    EXPECT_EQ(info.kind, slot_kind::this_adjustor);
    EXPECT_EQ(info.this_adjustment, -8);
    ASSERT_TRUE(info.final_target.has_value());
}

TEST(SlotClassifier, LargeThisAdjustorIsRecognized) {
    const vtable_view::slot_entry entries[] = {kLargeThisAdjustor};
    const vtable_view view{entries, 1};
    const slot_classifier classifier{view};
    const slot_info info = classifier.inspect(0);

    EXPECT_EQ(info.kind, slot_kind::this_adjustor);
    EXPECT_EQ(info.this_adjustment, -0x12345678);
    ASSERT_TRUE(info.final_target.has_value());
}

TEST(SlotClassifier, OrdinaryFunctionIsNotThisAdjustor) {
    const vtable_view::slot_entry entries[] = {kOrdinaryFunction};

    const vtable_view view{entries, 1};
    const slot_classifier classifier{view};

    const slot_info info = classifier.inspect(0);

    EXPECT_EQ(info.kind, slot_kind::direct);
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
}

TEST(SlotClassifier, DebugReturnAdjustorIsRecognized) {
    const vtable_view::slot_entry entries[] = {kMsvcDebugReturnAdjustor};

    const vtable_view view{entries, 1};
    const slot_classifier classifier{view};

    const slot_info info = classifier.inspect(0);

    EXPECT_EQ(info.kind, slot_kind::return_adjustor);
    EXPECT_EQ(info.return_adjustment, 8);
    EXPECT_FALSE(info.is_virtual_thunk);
    ASSERT_TRUE(info.final_target.has_value());

    // Target matches the point immediately after the E8 instruction inside our
    // buffer
    const auto* expected_target =
        static_cast<const unsigned char*>(kMsvcDebugReturnAdjustor) + 14;
    EXPECT_EQ(*info.final_target, expected_target);
}

TEST(SlotClassifier, ReleaseLargeReturnAdjustorIsRecognized) {
    const vtable_view::slot_entry entries[] = {kMsvcReleaseLargeReturnAdjustor};

    const vtable_view view{entries, 1};
    const slot_classifier classifier{view};

    const slot_info info = classifier.inspect(0);

    EXPECT_EQ(info.kind, slot_kind::return_adjustor);
    EXPECT_EQ(info.return_adjustment, 0x12345678);
    EXPECT_FALSE(info.is_virtual_thunk);
    ASSERT_TRUE(info.final_target.has_value());
}

TEST(SlotClassifier, VirtualInheritanceReturnAdjustorIsRecognized) {
    const vtable_view::slot_entry entries[] = {kMsvcVirtualReturnAdjustor};

    const vtable_view view{entries, 1};
    const slot_classifier classifier{view};

    const slot_info info = classifier.inspect(0);

    EXPECT_EQ(info.kind, slot_kind::return_adjustor);
    EXPECT_TRUE(info.is_virtual_thunk);
    EXPECT_EQ(info.return_adjustment, 0); // Dynamic offset at runtime
    ASSERT_TRUE(info.final_target.has_value());
}

} // namespace