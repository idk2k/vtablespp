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

    const auto table = vtable_view::from_object(calculator, 2);

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
            std::printf(
                "[hook] single original_entry=%p\n",
                context.original_entry()
            );
            std::fflush(stdout);
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

TEST(StatefulSlotHook, IndependentHooksForDifferentObjects) {
    Calculator first;
    Calculator second;

    const auto first_table = vtable_view::from_object(first, 2);

    const auto second_table = vtable_view::from_object(second, 2);

    stateful_slot_hook<int(int&, int&)> first_hook;
    stateful_slot_hook<int(int&, int&)> second_hook;

    ASSERT_TRUE(first_hook.install(
        first_table,
        1,
        first,
        [](auto& context, void* object, int a, int b) {
            std::printf("[hook] lambda enter\n");
            std::fflush(stdout);

            std::printf(
                "[hook] lambda original_entry=%p\n",
                context.original_entry()
            );
            std::fflush(stdout);

            int mutable_a = a;
            int mutable_b = b;

            const int original =
                context.call_original_raw(object, mutable_a, mutable_b);

            std::printf("[hook] lambda original=%d\n", original);
            std::fflush(stdout);

            return original * 10;
        }
    ));

    ASSERT_TRUE(second_hook.install(
        second_table,
        1,
        second,
        [](auto& context, void* object, int a, int b) {
            int mutable_a = a;
            int mutable_b = b;

            const int original =
                context.call_original_raw(object, mutable_a, mutable_b);

            return original * 100;
        }
    ));

    int first_a = 1;
    int first_b = 2;

    const int first_hooked_result =
        vtablespp::call_slot<int(int&, int&)>(first, 1, first_a, first_b);

    EXPECT_EQ(first_hooked_result, 30);

    int second_a = 1;
    int second_b = 2;

    const int second_hooked_result =
        vtablespp::call_slot<int(int&, int&)>(second, 1, second_a, second_b);

    EXPECT_EQ(second_hooked_result, 300);

    first_hook.uninstall();
    second_hook.uninstall();

    int restored_first_a = 1;
    int restored_first_b = 2;

    const int first_restored_result = vtablespp::call_slot<int(int&, int&)>(
        first,
        1,
        restored_first_a,
        restored_first_b
    );

    EXPECT_EQ(first_restored_result, 3);

    int restored_second_a = 1;
    int restored_second_b = 2;

    const int second_restored_result = vtablespp::call_slot<int(int&, int&)>(
        second,
        1,
        restored_second_a,
        restored_second_b
    );

    EXPECT_EQ(second_restored_result, 3);
}

} // namespace