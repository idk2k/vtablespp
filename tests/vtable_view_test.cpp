#include <gtest/gtest.h>

#include <optional>

#include <vtablespp/vtablespp.hpp>
#include <vtablespp/core/vtable.hpp>

#include "fixtures.hpp"

namespace {

using namespace vtablespp_test;

TEST(VTable, PolymorphicObjectHasNonZeroVTable) {
    SingleDerived object;

    EXPECT_NE(vtablespp::vtable_of(object), nullptr);
}

TEST(VTable, SingleInheritancePrimaryTableIsAccessible) {
    SingleDerived object;

    const auto view =
        vtablespp::vtable_view::from_object(object, 3);

    EXPECT_FALSE(view.empty());
    EXPECT_EQ(view.size(), 3u);
}

TEST(VTableView, DefaultViewIsEmpty) {
    const vtablespp::vtable_view view;

    EXPECT_TRUE(view.empty());
    EXPECT_EQ(view.size(), 0u);
    EXPECT_TRUE(view.entries().empty());
    EXPECT_EQ(view.at(0), std::nullopt);
}

TEST(VTableView, ExposesSuppliedEntries) {
    int first = 0;
    int second = 0;

    const vtablespp::vtable_view::slot_entry entries[] = {
        &first,
        &second
    };

    const vtablespp::vtable_view view{entries, 2};

    ASSERT_TRUE(view.at(0).has_value());
    ASSERT_TRUE(view.at(1).has_value());

    EXPECT_EQ(*view.at(0), &first);
    EXPECT_EQ(*view.at(1), &second);
    EXPECT_EQ(view.at(2), std::nullopt);
}

TEST(VTable, MultipleInheritanceExposesBothBaseTables) {
    MultiDerived object;

    const auto primary =
        vtablespp::vtable_view::from_object(object, 1);

    const auto secondary =
        vtablespp::vtable_view::from_base<Right>(object, 1);

    ASSERT_NE(primary.entries().data(), nullptr);
    ASSERT_NE(secondary.entries().data(), nullptr);

    EXPECT_NE(
        primary.entries().data(),
        secondary.entries().data()
    );
}

// ============ size detection ==================
TEST(VTable, DetectsPrimaryVtableSize) {
    SingleDerived object;

    const auto view =
        vtablespp::vtable_view::from_object_auto(object);

    EXPECT_FALSE(view.empty());
    EXPECT_GT(view.size(), 0u);

    for (std::size_t index = 0; index < view.size(); ++index) {
        EXPECT_TRUE(view.at(index).has_value());
    }

    EXPECT_EQ(view.at(view.size()), std::nullopt);
}

TEST(VTable, DetectsSecondaryVtableSize) {
    MultiDerived object;

    const auto secondary =
        vtablespp::vtable_view::from_base_auto<Right>(object);

    EXPECT_FALSE(secondary.empty());
    EXPECT_GT(secondary.size(), 0u);

    for (std::size_t index = 0; index < secondary.size(); ++index) {
        EXPECT_TRUE(secondary.at(index).has_value());
    }

    EXPECT_EQ(secondary.at(secondary.size()), std::nullopt);
}

} // namespace