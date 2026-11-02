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
    auto view = vtablespp::vtable_view::from_object(object, 3);
    EXPECT_FALSE(view.empty());
    EXPECT_EQ(view.size(), 3);
}

TEST(VTable, MultipleInheritanceHasSeparatePrimaryAndSecondaryTables) {
    MultiDerived object;

    const void* primary = vtablespp::vtable_of(object);

    // In the MSVC ABI, the secondary base usually has its own vftable pointer
    // after the primary base subobject. The exact offset must be validated
    // for the target architecture and compiler version.
    const auto* base = static_cast<const unsigned char*>(static_cast<const void*>(&object));
    const void* secondary = *reinterpret_cast<const void* const*>(base + sizeof(void*));

    EXPECT_NE(primary, nullptr);
    EXPECT_NE(secondary, nullptr);
}

TEST(VTableView, DefaultViewIsEmpty) {
    const vtablespp::vtable_view view;

    EXPECT_TRUE(view.empty());
    EXPECT_EQ(view.size(), 0u);
    EXPECT_TRUE(view.entries().empty());
    EXPECT_EQ(view.at(0), nullptr);
}

TEST(VTableView, ExposesSuppliedEntries) {
    int first = 0;
    int second = 0;

    const vtablespp::vtable_view::entry_type entries[] = {
        &first,
        &second
    };

    const vtablespp::vtable_view view{entries, 2};

    EXPECT_FALSE(view.empty());
    EXPECT_EQ(view.size(), 2u);

    EXPECT_EQ(view.at(0), &first);
    EXPECT_EQ(view.at(1), &second);
    EXPECT_EQ(view.at(2), nullptr);

    EXPECT_EQ(view.entries().data(), entries);
    EXPECT_EQ(view.entries().size(), 2u);
}

TEST(VTable, MultipleInheritanceExposesBothBaseTables) {
    using namespace vtablespp_test;

    MultiDerived object;

    const Left& left = object;
    const Right& right = object;

    const auto primary =
        vtablespp::vtable_view::from_object(left, 1);

    const auto secondary =
        vtablespp::vtable_view::from_object(right, 1);

    ASSERT_NE(primary.entries().data(), nullptr);
    ASSERT_NE(secondary.entries().data(), nullptr);

    EXPECT_NE(
        primary.entries().data(),
        secondary.entries().data()
    );

    EXPECT_NE(primary.at(0), nullptr);
    EXPECT_NE(secondary.at(0), nullptr);

    EXPECT_EQ(primary.at(1), nullptr);
    EXPECT_EQ(secondary.at(1), nullptr);
}

} // namespace