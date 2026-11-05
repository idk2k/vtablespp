#include <gtest/gtest.h>

#include <vtablespp/vtablespp.hpp>

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

TEST(VTable, MultipleInheritanceHasSeparatePrimaryAndSecondaryTables) {
    MultiDerived object;

    const void* primary =
        vtablespp::vtable_of(object);

    const auto secondary_view =
        vtablespp::vtable_view::from_base<Right>(object, 1);

    const void* secondary =
        static_cast<const void*>(secondary_view.entries().data());

    EXPECT_NE(primary, nullptr);
    EXPECT_NE(secondary, nullptr);
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

    const auto primary =
        vtablespp::vtable_view::from_object(left, 1);

    const auto secondary =
        vtablespp::vtable_view::from_base<Right>(object, 1);

    ASSERT_NE(primary.entries().data(), nullptr);
    ASSERT_NE(secondary.entries().data(), nullptr);

    EXPECT_NE(
        primary.entries().data(),
        secondary.entries().data()
    );

    ASSERT_TRUE(primary.at(0).has_value());
    ASSERT_TRUE(secondary.at(0).has_value());

    EXPECT_NE(*primary.at(0), nullptr);
    EXPECT_NE(*secondary.at(0), nullptr);

    EXPECT_EQ(primary.at(1), std::nullopt);
    EXPECT_EQ(secondary.at(1), std::nullopt);
}

} // namespace