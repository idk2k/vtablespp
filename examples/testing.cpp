#include <iostream>

struct Left {
    virtual void left() const {
        std::cout << "Left\n";
    }
};

struct Right {
    virtual void print() const {
        std::cout << "Right\n";
    }
};

struct Derived : Left, Right {
    void print() const override {
        std::cout << "Derived\n";
    }
};

#include <vtablespp/vtablespp.hpp>
#include <vtablespp/core/call_slot.hpp>

struct SingleBase {
    virtual ~SingleBase() = default;
    virtual int test() const {
        return 1;
    }
};

struct SingleDerived final : SingleBase {
    int test() const override {
        return 2;
    }
};

SingleDerived object;

struct Calculator {
    virtual int add(int a, int b) const {
        return a + b;
    }

    virtual ~Calculator() = default;
};

Calculator calculator;

const int sum = vtablespp::call_slot<int(int, int)>(calculator, 0, 2, 3);

int main() {
    // virtual int test() const
    const int result = vtablespp::call_slot<int()>(object, 1);

    const int sum = vtablespp::call_slot<int(int, int)>(calculator, 0, 2, 3);

    return 0;
}
