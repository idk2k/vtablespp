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

    virtual int subtract(int& a, int& b) {
        return a - b;
    }
};

TEST(StatefulSlotHook, CapturesStateAndCallsOriginal) {
    Calculator calculator;

    const auto table = vtable_view::from_object(calculator, 2);

    stateful_slot_hook<int(int&, int&)> hook;

    ASSERT_TRUE(hook.install(
        table,
        1,
        calculator,
        [](auto& context, void* object, int& a, int& b) {
            a *= 10;
            b *= 10;

            return context.call_original_raw(object, a, b);
        }
    ));

    int a = 1;
    int b = 2;

    const int result =
        vtablespp::call_slot<int(int&, int&)>(calculator, 1, a, b);

    EXPECT_EQ(a, 10);
    EXPECT_EQ(b, 20);
    EXPECT_EQ(result, 30);

    hook.uninstall();

    a = 1;
    b = 2;

    EXPECT_EQ(vtablespp::call_slot<int(int&, int&)>(calculator, 1, a, b), 3);

    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2);
}

TEST(StatefulSlotHook, RejectsSecondHookForSameObject) {
    Calculator calculator;

    const auto table = vtable_view::from_object(calculator, 2);

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

TEST(StatefulSlotHook, IndependentHooksForDifferentSlots) {
    Calculator calculator;

    const auto table = vtable_view::from_object(calculator, 3);

    stateful_slot_hook<int(int&, int&)> add_hook;
    stateful_slot_hook<int(int&, int&)> subtract_hook;

    ASSERT_TRUE(add_hook.install(
        table,
        1,
        calculator,
        [](auto& context, void* object, int& a, int& b) {
            int mutable_a = a;
            int mutable_b = b;

            const int original =
                context.call_original_raw(object, mutable_a, mutable_b);

            return original * 10;
        }
    ));

    ASSERT_TRUE(subtract_hook.install(
        table,
        2,
        calculator,
        [](auto& context, void* object, int& a, int& b) {
            int mutable_a = a;
            int mutable_b = b;

            const int original =
                context.call_original_raw(object, mutable_a, mutable_b);

            return original * 100;
        }
    ));

    int a = 1;
    int b = 2;

    EXPECT_EQ(vtablespp::call_slot<int(int&, int&)>(calculator, 1, a, b), 30);

    EXPECT_EQ(vtablespp::call_slot<int(int&, int&)>(calculator, 2, a, b), -100);

    add_hook.uninstall();
    subtract_hook.uninstall();

    EXPECT_EQ(vtablespp::call_slot<int(int&, int&)>(calculator, 1, a, b), 3);

    EXPECT_EQ(vtablespp::call_slot<int(int&, int&)>(calculator, 2, a, b), -1);
}

TEST(StatefulSlotHook, MultipleHooksOnDifferentSlots) {
    Calculator calculator;

    const auto table = vtable_view::from_object(calculator, 3);

    stateful_slot_hook<int(int&, int&)> add_hook;
    stateful_slot_hook<int(int&, int&)> subtract_hook;

    ASSERT_TRUE(add_hook.install(
        table,
        1,
        calculator,
        [](auto& context, void* object, int& a, int& b) {
            int mutable_a = a;
            int mutable_b = b;

            const int original =
                context.call_original_raw(object, mutable_a, mutable_b);

            return original * 10;
        }
    ));

    ASSERT_TRUE(subtract_hook.install(
        table,
        2,
        calculator,
        [](auto& context, void* object, int& a, int& b) {
            int mutable_a = a;
            int mutable_b = b;

            const int original =
                context.call_original_raw(object, mutable_a, mutable_b);

            return original * 100;
        }
    ));

    int a = 1;
    int b = 2;

    EXPECT_EQ(vtablespp::call_slot<int(int&, int&)>(calculator, 1, a, b), 30);

    EXPECT_EQ(vtablespp::call_slot<int(int&, int&)>(calculator, 2, a, b), -100);

    add_hook.uninstall();

    EXPECT_EQ(vtablespp::call_slot<int(int&, int&)>(calculator, 1, a, b), 3);

    EXPECT_EQ(vtablespp::call_slot<int(int&, int&)>(calculator, 2, a, b), -100);

    subtract_hook.uninstall();

    EXPECT_EQ(vtablespp::call_slot<int(int&, int&)>(calculator, 2, a, b), -1);
}

} // namespace