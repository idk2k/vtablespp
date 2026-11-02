#include <iostream>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

#include <vtablespp/vtablespp.hpp>
#include "support/print_vtable.hpp"

namespace {

struct Left {
    virtual int left() const {
        return 1;
    }
};

struct Right {
    virtual int right() const {
        return 2;
    }
};

struct Derived : Left, Right {
    int left() const override {
        return 10;
    }

    int right() const override {
        return 20;
    }

    virtual int own() const {
        return 30;
    }
};

constexpr std::size_t inspected_slot_count = 1;

void show_multiple_inheritance() {
    Derived object;

    const Left& left = object;
    const Right& right = object;

    std::cout << "\n=== Multiple inheritance ===\n";

    std::cout << "\nComplete object address: "
              << static_cast<const void*>(std::addressof(object))
              << '\n';

    std::cout << "sizeof(MultiDerived):    "
              << sizeof(object)
              << " bytes\n";

    print_table("Left subobject", left);
    print_table("Right subobject", right);

    std::cout << "\nNormal virtual calls:\n";
    std::cout << "  left.left():   " << left.left() << '\n';
    std::cout << "  right.right(): " << right.right() << '\n';

    std::cout
        << "\nThese are two base subobjects of ONE complete object.\n"
        << "Compare their addresses and their vftable addresses.\n";
}

}

int main() {
    show_multiple_inheritance();
}