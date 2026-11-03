#include <cstddef>
#include <iostream>

#include <vtablespp/vtablespp.hpp>

#include "support/print_vtable.hpp"

namespace {

struct Left {
    virtual void left() const {
        std::cout << "    left() this: "
                  << static_cast<const void*>(this) << '\n';
    }
};

struct Right {
    virtual Right* clone() const {
        return new Right{*this};
    }

    virtual int right() const {
        return 123;
    }
};

struct Derived : Left, Right {
    Derived* clone() const override {
        std::cout << "    clone() this:  "
                  << static_cast<const void*>(this) << '\n';
        return new Derived{*this};
    }

    int right() const override {
        return 1223312312;
    }
};

// A safe upper bound for this experiment. The real vftable length is not
// discovered or validated by vtablespp.
constexpr std::size_t inspected_slot_count = 5;

void print_pointer_conversions(const Derived& object) {
    const Right* right_pointer = &object;
    const Left* left_pointer = &object;

    std::cout << "\nPointer conversion:\n";
    std::cout << "  Derived* address: "
              << static_cast<const void*>(&object) << '\n';
    std::cout << "  Left* address:    "
              << static_cast<const void*>(left_pointer) << '\n';
    std::cout << "  Right* address:   "
              << static_cast<const void*>(right_pointer) << '\n';
}

void show_multiple_inheritance() {
    Derived object;

    const Left& left = object;
    const Right& right = object;

    Derived* d = &object;
    Right* r = &object;

    std::cout << "=== Multiple inheritance ===\n";

    std::cout << "\nComplete object address: "
              << static_cast<const void*>(std::addressof(object)) << '\n';
    std::cout << "sizeof(Derived):         "
              << sizeof(object) << " bytes\n";

    vtablespp_example::print_object_vtable(
        "Left subobject",
        left,
        inspected_slot_count
    );

    vtablespp_example::print_object_vtable(
        "Right subobject",
        right,
        inspected_slot_count
    );

    print_pointer_conversions(object);

    std::cout << "\nNormal virtual calls:\n";

    std::cout << "  left.left():\n";
    left.left();

    std::cout << "  right.right(): "
              << right.right() << '\n';

    std::cout << "  d->right():    "
              << d->right() << '\n';

    std::cout << "\nClone through Derived*:\n";
    std::cout << "  d->clone():\n";
    Derived* derived_clone = d->clone();

    std::cout << "  returned Derived*: "
              << static_cast<const void*>(derived_clone) << '\n';

    // The static type of the returned pointer is Derived*,
    // so deletion through it is well-formed.
    delete derived_clone;

    std::cout << "\nClone through Right*:\n";
    std::cout << "  r->clone():\n";
    Right* right_clone = r->clone();

    std::cout << "  returned Right*:   "
              << static_cast<const void*>(right_clone) << '\n';

    // Demonstration-only cleanup. The returned pointer is Right*,
    // and Right has no virtual destructor, so it must not be deleted
    // through Right*. We know the dynamic type in this experiment.
    delete static_cast<Derived*>(right_clone);

    std::cout << "\nInspect the printed vftable addresses in the debugger.\n";
}

} // namespace

int main() {
    show_multiple_inheritance();
}