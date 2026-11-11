#include <iostream>
#include <memory>

#include <vtablespp/core/slot_classifier.hpp>
#include <vtablespp/core/vtable.hpp>
#include <vtablespp/symbols/symbol_resolver.hpp>

namespace {

struct Left {
    virtual void Q() = 0;
    virtual ~Left() = default;
};

struct Right {
    virtual void Q() = 0;
    virtual ~Right() = default;
};

struct SharedDerived final : Left, Right {
    void Q() override {}
};

struct SeparateLeft {
    virtual void Q() {}
    virtual ~SeparateLeft() = default;
};

struct SeparateRight {
    virtual void Q() {}
    virtual ~SeparateRight() = default;
};

struct SeparateDerived final
    : SeparateLeft,
      SeparateRight {};

void print_symbol(const vtablespp::slot_info& info) {
    std::cout << "  kind: ";

    switch (info.kind) {
    case vtablespp::slot_kind::direct:
        std::cout << "direct";
        break;
    case vtablespp::slot_kind::this_adjustor:
        std::cout << "this_adjustor";
        break;
    case vtablespp::slot_kind::return_adjustor:
        std::cout << "return_adjustor";
        break;
    case vtablespp::slot_kind::pure_call:
        std::cout << "pure_call";
        break;
    default:
        std::cout << "unknown";
        break;
    }

    std::cout << '\n';

    if (info.symbol_name.has_value()) {
        std::cout << "  mangled: " << *info.symbol_name << '\n';
    }

    if (info.demangled_name.has_value()) {
        std::cout << "  demangled: " << *info.demangled_name << '\n';
    }

    if (info.module.has_value()) {
        std::cout << "  module: " << *info.module << '\n';
    }

    std::cout << "  displacement: "
              << info.symbol_displacement
              << '\n';

    if (info.final_target.has_value()) {
        std::cout << "  final_target: "
                  << *info.final_target
                  << '\n';
    }
}

void print_separator(const char* title) {
    std::cout << "\n== " << title << " ==\n";
}

} // namespace

int main() {
    auto resolver =
        std::make_shared<vtablespp::symbols::symbol_resolver>();

    if (!resolver->initialize()) {
        std::cerr << "Failed to initialize DbgHelp symbol resolver\n";
        return 1;
    }

    print_separator("Direct virtual method");

    SeparateDerived object;

    const auto secondary_view =
        vtablespp::vtable_view::from_base<SeparateRight>(object, 1);

    const vtablespp::slot_classifier secondary_classifier{
        secondary_view,
        resolver
    };

    const auto direct_info = secondary_classifier.inspect(0);
    print_symbol(direct_info);

    print_separator("Shared override thunk");

    SharedDerived thunk_object;

    const auto right_view =
        vtablespp::vtable_view::from_base<Right>(thunk_object, 1);

    const vtablespp::slot_classifier right_classifier{
        right_view,
        resolver
    };

    const auto thunk_info = right_classifier.inspect(0);
    print_symbol(thunk_info);

    print_separator("Known function");

    const auto known_symbol = resolver->resolve(
        reinterpret_cast<const void*>(&print_separator)
    );

    if (known_symbol.has_value()) {
        std::cout << "  mangled: "
                  << known_symbol->name
                  << '\n';

        if (known_symbol->demangled_name.has_value()) {
            std::cout << "  demangled: "
                      << *known_symbol->demangled_name
                      << '\n';
        }

        if (known_symbol->module.has_value()) {
            std::cout << "  module: "
                      << *known_symbol->module
                      << '\n';
        }

        std::cout << "  displacement: "
                  << known_symbol->displacement
                  << '\n';
    } else {
        std::cout << "  Symbol was not resolved\n";
    }

    return 0;
}