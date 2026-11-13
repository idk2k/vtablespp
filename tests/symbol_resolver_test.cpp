#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <vtablespp/core/slot_classifier.hpp>
#include <vtablespp/core/vtable.hpp>
#include <vtablespp/symbols/symbol_resolver.hpp>

#include "fixtures.hpp"

namespace {

using namespace vtablespp_test;
using vtablespp::direct_evidence;
using vtablespp::slot_classifier;
using vtablespp::slot_kind;
using vtablespp::vtable_view;
using vtablespp::symbols::symbol_resolver;

TEST(SymbolResolver, InitializeSucceeds) {
    symbol_resolver resolver;

    EXPECT_TRUE(resolver.initialize());
}

TEST(SymbolResolver, ResolveDirectMethod) {
    SeparateImplementationsDerived object;

    auto resolver = std::make_shared<symbol_resolver>();
    ASSERT_TRUE(resolver->initialize());

    const auto secondary = vtable_view::from_base<SeparateRight>(object, 1);

    const slot_classifier classifier{secondary, resolver};
    const auto info = classifier.inspect(0);

    EXPECT_EQ(info.kind, slot_kind::direct);
    EXPECT_EQ(info.evidence, direct_evidence::symbol_resolved);

    ASSERT_TRUE(info.symbol_name.has_value());
    ASSERT_TRUE(info.demangled_name.has_value());
    ASSERT_TRUE(info.module.has_value());

    EXPECT_EQ(info.symbol_displacement, 0u);

    EXPECT_NE(info.demangled_name->find("SeparateRight::Q"), std::string::npos);
}

TEST(SymbolResolver, ResolveThunkTarget) {
    struct Left {
        virtual void Q() = 0;
        virtual ~Left() = default;
    };

    struct Right {
        virtual void Q() = 0;
        virtual ~Right() = default;
    };

    struct Derived final : Left, Right {
        void Q() override {}
    };

    Derived object;

    auto resolver = std::make_shared<symbol_resolver>();
    ASSERT_TRUE(resolver->initialize());

    const auto secondary = vtable_view::from_base<Right>(object, 1);

    const slot_classifier classifier{secondary, resolver};
    const auto info = classifier.inspect(0);

    EXPECT_EQ(info.kind, slot_kind::this_adjustor);
    EXPECT_EQ(
        info.this_adjustment,
        -static_cast<std::ptrdiff_t>(sizeof(void*))
    );
    ASSERT_TRUE(info.final_target.has_value());

    ASSERT_TRUE(info.symbol_name.has_value());
    ASSERT_TRUE(info.demangled_name.has_value());

    EXPECT_NE(info.demangled_name->find("Derived::Q"), std::string::npos);
}

static void SomeTestFunction() {}

TEST(SymbolResolver, ResolvesKnownFunction) {
    symbol_resolver resolver;
    ASSERT_TRUE(resolver.initialize());

    const auto symbol =
        resolver.resolve(reinterpret_cast<const void*>(&SomeTestFunction));

    ASSERT_TRUE(symbol.has_value())
        << "SymFromAddr failed; check PDB generation/loading";

    EXPECT_FALSE(symbol->name.empty());
}

} // namespace