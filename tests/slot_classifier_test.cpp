#include <gtest/gtest.h>

#include <array>

#include <vtablespp/vtablespp.hpp>
#include <vtablespp/core/slot_classifier.hpp>

namespace {

using vtablespp::slot_classifier;
using vtablespp::slot_kind;
using vtablespp::slot_info;
using vtablespp::vtable_view;

constexpr unsigned char kSmallThisAdjustor[] = {
    0x48, 0x83, 0xE9, 0x08,
    0xE9, 0x10, 0x00, 0x00, 0x00
};

constexpr unsigned char kLargeThisAdjustor[] = {
    0x48, 0x81, 0xE9, 0x78, 0x56, 0x34, 0x12,
    0xE9, 0x10, 0x00, 0x00, 0x00
};

constexpr unsigned char kOrdinaryFunction[] = {
    0x48, 0x89, 0x4C, 0x24, 0x08,
    0x48, 0x83, 0xEC, 0x28
};

TEST(SlotClassifier, SmallThisAdjustorIsRecognized) {
    const vtable_view::slot_entry entries[] = {
        kSmallThisAdjustor
    };

    const vtable_view view{entries, 1};
    const slot_classifier classifier{view};

    const slot_info info = classifier.inspect(0);

    EXPECT_EQ(info.kind, slot_kind::this_adjustor);
    EXPECT_EQ(info.this_adjustment, 8);
    ASSERT_TRUE(info.final_target.has_value());
}

TEST(SlotClassifier, LargeThisAdjustorIsRecognized) {
    const vtable_view::slot_entry entries[] = {
        kLargeThisAdjustor
    };

    const vtable_view view{entries, 1};
    const slot_classifier classifier{view};

    const slot_info info = classifier.inspect(0);

    EXPECT_EQ(info.kind, slot_kind::this_adjustor);
    EXPECT_EQ(info.this_adjustment, 0x12345678);
    ASSERT_TRUE(info.final_target.has_value());
}

TEST(SlotClassifier, OrdinaryFunctionIsNotThisAdjustor) {
    const vtable_view::slot_entry entries[] = {
        kOrdinaryFunction
    };

    const vtable_view view{entries, 1};
    const slot_classifier classifier{view};

    const slot_info info = classifier.inspect(0);

    EXPECT_EQ(info.kind, slot_kind::unknown);
    EXPECT_EQ(info.this_adjustment, 0);
    EXPECT_EQ(info.final_target, std::nullopt);
}

TEST(SlotClassifier, OutOfRangeSlotIsUnknown) {
    const vtable_view::slot_entry entries[] = {
        kSmallThisAdjustor
    };

    const vtable_view view{entries, 1};
    const slot_classifier classifier{view};

    const slot_info info = classifier.inspect(1);

    EXPECT_EQ(info.kind, slot_kind::unknown);
    EXPECT_EQ(info.entry, nullptr);
}

} // namespace