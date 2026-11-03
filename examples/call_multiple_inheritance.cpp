#include <cstddef>
#include <iostream>

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

constexpr std::size_t initial_slot_count = 1;

void show_multiple_inheritance() {
    Derived object;

    const Left& left = object;
    const Right& right = object;

    std::cout << "=== Multiple inheritance: own virtual method ===\n";

    std::cout << "\nComplete object address: ";
    std::cout << static_cast<const void*>(std::addressof(object)) << '\n';

    std::cout << "sizeof(Derived):         ";
    std::cout << sizeof(object) << " bytes\n";

    vtablespp_example::print_object_vtable(
        "Left subobject",
        left,
        initial_slot_count
    );

    vtablespp_example::print_object_vtable(
        "Right subobject",
        right,
        initial_slot_count
    );

    std::cout << "\nNormal virtual calls:\n";
    std::cout << "  left.left():   " << left.left() << '\n';
    std::cout << "  right.right(): " << right.right() << '\n';
    std::cout << "  object.own():  " << object.own() << '\n';

    std::cout << "\nNext step:\n";
    std::cout << "  Increase the inspected slot count gradually,\n";
    std::cout << "  then compare table entries with symbols in the debugger.\n";
}

} // namespace

int main() {
    show_multiple_inheritance();
}