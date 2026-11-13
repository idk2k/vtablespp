#include <gtest/gtest.h>

#include <vtablespp/vtablespp.hpp>
#include <vtablespp/core/slot_hook.hpp>
#include <vtablespp/core/vtable.hpp>
#include <vtablespp/core/call_slot.hpp>

namespace {

using vtablespp::slot_hook;
using vtablespp::vtable_view;

struct Calculator {
    virtual ~Calculator() = default;
    virtual int add(int& a, int& b) {
        return a + b;
    }
};

using add_hook_type = slot_hook<int(int&, int&)>;
using add_function_type = add_hook_type::function_type;

add_function_type original_add = nullptr;
int before_call_count = 0;
int after_call_count = 0;

int hooked_add(void* object, int& a, int& b) {
    ++before_call_count;

    a += 100;
    b += 200;

    const int original_result = original_add(object, a, b);

    ++after_call_count;

    return original_result + 1000;
}

TEST(SlotHook, HooksReferenceSlotChangesArgumentsAndRestoresOriginal) {
    Calculator calculator;

    const auto table = vtable_view::from_object(calculator, 2);

    ASSERT_TRUE(table.at(1).has_value());

    const void* original_entry = *table.at(1);

    original_add =
        reinterpret_cast<add_function_type>(const_cast<void*>(original_entry));

    before_call_count = 0;
    after_call_count = 0;

    add_hook_type hook;
    ASSERT_TRUE(hook.install(table, 1, &hooked_add));
    EXPECT_TRUE(hook.installed());
    EXPECT_EQ(hook.original_entry(), original_entry);

    int a = 1;
    int b = 2;

    const int result =
        vtablespp::call_slot<int(int&, int&)>(calculator, 1, a, b);

    EXPECT_EQ(before_call_count, 1);
    EXPECT_EQ(after_call_count, 1);

    EXPECT_EQ(a, 101);
    EXPECT_EQ(b, 202);
    EXPECT_EQ(result, 1303);

    hook.uninstall();

    EXPECT_FALSE(hook.installed());
    EXPECT_EQ(*table.at(1), original_entry);

    before_call_count = 0;
    after_call_count = 0;

    a = 1;
    b = 2;

    EXPECT_EQ(vtablespp::call_slot<int(int&, int&)>(calculator, 1, a, b), 3);
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2);
    EXPECT_EQ(before_call_count, 0);
    EXPECT_EQ(after_call_count, 0);
}

TEST(SlotHook, RejectsNullReplacement) {
    Calculator calculator;

    const auto table = vtable_view::from_object(calculator, 2);

    add_hook_type hook;

    EXPECT_FALSE(hook.install(table, 1, nullptr));
    EXPECT_FALSE(hook.installed());
}

TEST(SlotHook, RejectsOutOfRangeSlot) {
    Calculator calculator;

    const auto table = vtable_view::from_object(calculator, 2);

    add_hook_type hook;

    EXPECT_FALSE(hook.install(table, 2, &hooked_add));
    EXPECT_FALSE(hook.installed());
}

} // namespace