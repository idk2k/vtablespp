#include <gtest/gtest.h>

#include <vtablespp/vtablespp.hpp>
#include <vtablespp/core/slot_classifier.hpp>

#include "fixtures.hpp"

namespace {

using namespace vtablespp_test;

using vtablespp::slot_classifier;
using vtablespp::slot_kind;
using vtablespp::vtable_view;

TEST(MsvcVTable, SharedOverrideCreatesSecondaryThisAdjustor) {

    struct Left {
    virtual void Q() = 0;
    virtual ~Left() = default;
    };

    struct Right {
        virtual void Q() = 0;
        virtual ~Right() = default;
    };

    struct MultiDerived final : Left, Right {
        void Q() override {}
    };
    MultiDerived object;

    const auto secondary =
        vtable_view::from_base<Right>(object, 1);

    const slot_classifier classifier{secondary};

    const auto info = classifier.inspect(0);

    EXPECT_EQ(info.kind, slot_kind::this_adjustor);
    EXPECT_EQ(info.this_adjustment, sizeof(void*));
    ASSERT_TRUE(info.final_target.has_value());
}

TEST(MsvcVTable, SeparateBaseImplementationsHaveNoThisAdjustor) {
    SeparateImplementationsDerived object;

    const auto secondary =
        vtable_view::from_base<SeparateRight>(object, 1);

    const slot_classifier classifier{secondary};

    const auto info = classifier.inspect(0);

    // Пока direct не реализован, корректный консервативный
    // результат для прямой функции — unknown.
    EXPECT_EQ(info.kind, slot_kind::unknown);
    EXPECT_EQ(info.this_adjustment, 0);
    EXPECT_EQ(info.final_target, std::nullopt);
}



} // namespace