#include <iostream>

#include <vtablespp/vtablespp.hpp>
#include <vtablespp/core/slot_classifier.hpp>
#include <vtablespp/core/vtable.hpp>

namespace {

struct Animal {
    virtual ~Animal() = default;
    virtual const char* name() const {
        return "Animal";
    }
};

struct Dog final : Animal {
    const char* name() const override {
        return "Dog";
    }
};

struct Left {
    virtual ~Left() = default;
    virtual int left() const {
        return 1;
    }
};

struct Right {
    virtual ~Right() = default;
    virtual int right() const {
        return 2;
    }
};

struct Multi final : Left, Right {
    int left() const override {
        return 10;
    }

    int right() const override {
        return 20;
    }
};

void print_view(const char* title, const vtablespp::vtable_view& view) {
    std::cout << title << '\n';
    std::cout << "  address: " << view.vtable_address() << '\n';
    std::cout << "  detected slots: " << view.size() << '\n';

    const vtablespp::slot_classifier classifier{view};

    for (std::size_t index = 0; index < view.size(); ++index) {
        const auto info = classifier.inspect(index);

        std::cout << "  [" << index << "] ";

        switch (info.kind) {
        case vtablespp::slot_kind::direct:
            std::cout << "direct";
            break;
        case vtablespp::slot_kind::this_adjustor:
            std::cout << "this-adjustor";
            break;
        case vtablespp::slot_kind::return_adjustor:
            std::cout << "return-adjustor";
            break;
        case vtablespp::slot_kind::pure_call:
            std::cout << "pure-call";
            break;
        case vtablespp::slot_kind::unknown:
            std::cout << "unknown";
            break;
        }

        if (info.demangled_name) {
            std::cout << " -> " << *info.demangled_name;
        }

        std::cout << '\n';
    }

    std::cout << '\n';
}

} // namespace

int main() {
    Dog dog;
    Multi multi;

    const auto dog_vtable = vtablespp::vtable_view::from_object_auto(dog);

    print_view("Dog primary vtable", dog_vtable);

    const auto left_vtable = vtablespp::vtable_view::from_object_auto(multi);

    const auto right_vtable =
        vtablespp::vtable_view::from_base_auto<Right>(multi);

    print_view("Multi primary vtable", left_vtable);
    print_view("Multi secondary vtable", right_vtable);

    return 0;
}