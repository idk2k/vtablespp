#include <gtest/gtest.h>
#include <stdexcept>
#include <vtablespp/vtablespp.hpp>
#include <vtablespp/core/stateful_slot_hook.hpp>
#include <vtablespp/core/vtable.hpp>
#include <vtablespp/core/call_slot.hpp>

#include <vtablespp/core/slot_classifier.hpp>
#include <vtablespp/symbols/symbol_resolver.hpp>
#include <vtablespp/core/slot_info.hpp>

namespace {

using vtablespp::slot_context;
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

struct Left {
    virtual ~Left() = default;
    virtual int left() const {
        return 10;
    }
};

struct Right {
    virtual ~Right() = default;
    virtual int right() const {
        return 20;
    }
};

struct MultiDerived final : Left, Right {
    int left() const override {
        return 100;
    }
    int right() const override {
        return 200;
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

TEST(StatefulSlotHook, HookDoesNotFireAfterUninstall) {
    Calculator calculator;
    const auto table = vtable_view::from_object_auto(calculator);
    stateful_slot_hook<int(int&, int&)> hook;

    int call_count = 0;
    ASSERT_TRUE(hook.install(
        table,
        1,
        calculator,
        [&call_count](auto& context, void* object, int& a, int& b) {
            ++call_count;
            return context.call_original_raw(object, a, b);
        }
    ));

    int a = 1, b = 2;
    vtablespp::call_slot<int(int&, int&)>(calculator, 1, a, b);
    EXPECT_EQ(call_count, 1);

    hook.uninstall();
    a = 1;
    b = 2;
    vtablespp::call_slot<int(int&, int&)>(calculator, 1, a, b);
    EXPECT_EQ(call_count, 1); // callback больше не вызывается
}

TEST(StatefulSlotHook, CallbackCanSkipOriginal) {
    Calculator calculator;
    const auto table = vtable_view::from_object_auto(calculator);
    stateful_slot_hook<int(int&, int&)> hook;

    ASSERT_TRUE(hook.install(
        table,
        1,
        calculator,
        [](auto& context, void* object, int& a, int& b) {
            // не вызываем оригинал — возвращаем своё
            return 999;
        }
    ));

    int a = 1, b = 2;
    const int result =
        vtablespp::call_slot<int(int&, int&)>(calculator, 1, a, b);
    EXPECT_EQ(result, 999);
    EXPECT_EQ(a, 1); // оригинал не менял аргументы
    EXPECT_EQ(b, 2);
}

TEST(StatefulSlotHook, MultipleInstallUninstallCycles) {
    Calculator calculator;
    const auto table = vtable_view::from_object_auto(calculator);
    const void* original = *table.at(1);

    for (int i = 0; i < 10; ++i) {
        stateful_slot_hook<int(int&, int&)> hook;
        ASSERT_TRUE(hook.install(
            table,
            1,
            calculator,
            [i](auto& context, void* object, int& a, int& b) {
                a += i;
                return context.call_original_raw(object, a, b);
            }
        ));

        // Проверяем, что слот указывает не на оригинал (т.е. hook установлен)
        EXPECT_NE(*table.at(1), original);

        int a = 1, b = 2;
        const int result =
            vtablespp::call_slot<int(int&, int&)>(calculator, 1, a, b);
        EXPECT_EQ(result, 3 + i);

        hook.uninstall();
        // После uninstall слот восстановлен
        EXPECT_EQ(*table.at(1), original);
    }
}

TEST(StatefulSlotHook, SecondHookOnSameSlotDifferentObjectRejected) {
    Calculator first;
    Calculator second;
    const auto table = vtable_view::from_object_auto(first);
    stateful_slot_hook<int(int&, int&)> hook1;
    stateful_slot_hook<int(int&, int&)> hook2;

    ASSERT_TRUE(
        hook1.install(table, 1, first, [](auto& c, void* o, int& a, int& b) {
            return c.call_original_raw(o, a, b);
        })
    );
    // слот один — второй hook должен отклониться
    EXPECT_FALSE(
        hook2.install(table, 1, second, [](auto& c, void* o, int& a, int& b) {
            return c.call_original_raw(o, a, b);
        })
    );
}

TEST(StatefulSlotHook, SecondaryVtableHook) {
    MultiDerived object;
    const auto secondary = vtable_view::from_base<Right>(object, 2);

    Right& right_ref = object;

    // 1. Прямой вызов — baseline (проверяем, что right() работает)
    const int direct = right_ref.right();
    std::printf("[test] direct right() = %d\n", direct);
    std::fflush(stdout);
    ASSERT_EQ(direct, 200);

    // 2. call_slot БЕЗ hook — slot 1 (right(), не destructor)
    const int via_slot = vtablespp::call_slot<int()>(right_ref, 1);
    std::printf("[test] via_slot right() = %d\n", via_slot);
    std::fflush(stdout);
    ASSERT_EQ(via_slot, 200);

    // 3. Hook на slot 1 (right())
    stateful_slot_hook<int()> hook;
    ASSERT_TRUE(
        hook.install(secondary, 1, object, [](auto& context, void* obj) {
            const int original = context.call_original_raw(obj);
            std::printf("[test] callback original = %d\n", original);
            std::fflush(stdout);
            return original + 100;
        })
    );

    // 4. call_slot С hook — slot 1
    const int result = vtablespp::call_slot<int()>(right_ref, 1);
    std::printf("[test] result = %d\n", result);
    std::fflush(stdout);
    EXPECT_EQ(result, 300);  // 200 + 100

    // 5. Uninstall — возвращаем оригинал
    hook.uninstall();
    const int after = vtablespp::call_slot<int()>(right_ref, 1);
    std::printf("[test] after uninstall = %d\n", after);
    std::fflush(stdout);
    EXPECT_EQ(after, 200);
}

TEST(StatefulSlotHook, ExceptionFromCallbackPropagates) {
    Calculator calculator;
    const auto table = vtable_view::from_object_auto(calculator);
    stateful_slot_hook<int(int&, int&)> hook;

    ASSERT_TRUE(hook.install(
        table,
        1,
        calculator,
        [](auto& context, void* object, int& a, int& b) -> int {
            throw std::runtime_error("hook error");
        }
    ));

    int a = 1, b = 2;
    EXPECT_THROW(
        vtablespp::call_slot<int(int&, int&)>(calculator, 1, a, b),
        std::runtime_error
    );
    hook.uninstall();
}

} // namespace