#include <gtest/gtest.h>

#include <vtablespp/vtablespp.hpp>
#include <vtablespp/core/stateful_slot_hook.hpp>
#include <vtablespp/core/vtable.hpp>
#include <vtablespp/core/call_slot.hpp>

namespace {

using vtablespp::stateful_slot_hook;
using vtablespp::vtable_view;

struct Calculator {
    virtual ~Calculator() = default;
    virtual int add(int& a, int& b) {
        return a + b;
    }
};

TEST(StatefulSlotHook, CapturesStateAndCallsOriginal) {
    Calculator calculator;

    const auto table =
        vtable_view::from_object(calculator, 2);

    stateful_slot_hook<int(int&, int&)> hook;

    ASSERT_TRUE(hook.install(
        table,
        1,
        calculator,
        [multiplier = 10](
            vtablespp::slot_context<int(int&, int&)>& context,
            void* object,
            int& a,
            int& b
        ) {
            a *= multiplier;
            b *= multiplier;

            return context.call_original_raw(object, a, b);
        }
    ));

    int a = 1;
    int b = 2;

    const int result =
        vtablespp::call_slot<int(int&, int&)>(
            calculator,
            1,
            a,
            b
        );

    EXPECT_EQ(a, 10);
    EXPECT_EQ(b, 20);
    EXPECT_EQ(result, 30);

    hook.uninstall();

    a = 1;
    b = 2;

    EXPECT_EQ(
        vtablespp::call_slot<int(int&, int&)>(
            calculator,
            1,
            a,
            b
        ),
        3
    );

    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2);
}

TEST(StatefulSlotHook, RejectsSecondHookForSameObject) {
    Calculator calculator;

    const auto table =
        vtable_view::from_object(calculator, 2);

    stateful_slot_hook<int(int&, int&)> first;
    stateful_slot_hook<int(int&, int&)> second;

    EXPECT_TRUE(first.install(
        table,
        1,
        calculator,
        [](auto& context, void* object, int& a, int& b) {
            return context.call_original_raw(object, a, b);
        }
    ));

    EXPECT_FALSE(second.install(
        table,
        1,
        calculator,
        [](auto& context, void* object, int& a, int& b) {
            return context.call_original_raw(object, a, b);
        }
    ));

    first.uninstall();

    EXPECT_TRUE(second.install(
        table,
        1,
        calculator,
        [](auto& context, void* object, int& a, int& b) {
            return context.call_original_raw(object, a, b);
        }
    ));

    second.uninstall();
}

}