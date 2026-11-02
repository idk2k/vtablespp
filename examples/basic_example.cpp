#include <iostream>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

#include <vtablespp/vtablespp.hpp>

namespace {

struct Base {
    virtual int value() const {
        return 10;
    }
};

struct Derived : Base {
    int value() const override {
        return 20;
    }
};

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

struct MultiDerived : Left, Right {
    int left() const override {
        return 100;
    }

    int right() const override {
        return 200;
    }
};

constexpr std::size_t inspected_slot_count = 1;

template <class T>
void print_table(std::string_view label, const T& object) {
    const auto view = vtablespp::vtable_view::from_object(
        object,
        inspected_slot_count
    );

    std::cout << "\n[" << label << "]\n";

    std::cout << "  Object/subobject address: "
              << static_cast<const void*>(std::addressof(object))
              << '\n';

    std::cout << "  Vftable address:          "
              << static_cast<const void*>(view.entries().data())
              << '\n';

    std::cout << "  Inspected slots:          "
              << view.size()
              << '\n';

    for (std::size_t index = 0; index < view.size(); ++index) {
        std::cout << "  slot[" << index << "]:                  "
                  << view.at(index)
                  << '\n';
    }
}

void show_single_inheritance() {
    Base base_object;
    Derived derived_object;

    const Base& base = base_object;
    const Base& derived_as_base = derived_object;

    std::cout << "\n=== Single inheritance ===\n";

    print_table("Base object", base);
    print_table("Derived object viewed as Base", derived_as_base);

    std::cout << "\nNormal virtual calls:\n";
    std::cout << "  Base::value():    " << base.value() << '\n';
    std::cout << "  Derived::value(): " << derived_as_base.value() << '\n';

    std::cout
        << "\nLook at the table addresses and slot[0] in both views.\n"
        << "Both calls use a Base reference, but the objects differ.\n";
}

void show_multiple_inheritance() {
    MultiDerived object;

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

void print_menu() {
    std::cout
        << "\n=== vtablespp laboratory ===\n"
        << "1 - Single inheritance\n"
        << "2 - Multiple inheritance\n"
        << "0 - Exit\n"
        << "> ";
}

}

int main() {
    std::cout
        << "MSVC ABI experiment.\n"
        << "Slot counts are supplied explicitly.\n"
        << "Extracted addresses are displayed, not called.\n";

    std::string command;

    while (true) {
        print_menu();

        if (!std::getline(std::cin, command)) {
            break;
        }

        if (command == "0") {
            break;
        }

        if (command == "1") {
            show_single_inheritance();
        } else if (command == "2") {
            show_multiple_inheritance();
        } else {
            std::cout << "Unknown command. Enter 1, 2 or 0.\n";
        }
    }
}