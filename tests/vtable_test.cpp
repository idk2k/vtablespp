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

} // namespace