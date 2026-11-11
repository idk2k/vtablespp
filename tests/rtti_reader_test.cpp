#include <gtest/gtest.h>

#include <string>

#include <vtablespp/rtti/rtti_reader.hpp>
#include <vtablespp/vtablespp.hpp>

#include "fixtures.hpp"

namespace {

using vtablespp::rtti::rtti_reader;
using namespace vtablespp_test;

TEST(RttiReader, ReadsCompleteClassName) {
    SeparateImplementationsDerived object;

    const auto view =
        vtablespp::vtable_view::from_object(object, 1);

    rtti_reader reader{view.vtable_address()};
    const auto info = reader.read();

    EXPECT_EQ(info->mangled_class_name.rfind(".?A", 0), 0u)
    << "Unexpected RTTI name: [" << info->mangled_class_name << "]";
    ASSERT_TRUE(info->class_name.has_value())
    << "Raw RTTI name: [" << info->mangled_class_name << "]";
    ASSERT_TRUE(info.has_value());
    EXPECT_TRUE(info->valid);
    EXPECT_FALSE(info->mangled_class_name.empty());
    ASSERT_TRUE(info->class_name.has_value());

    EXPECT_NE(
        info->class_name->find("SeparateImplementationsDerived"),
        std::string::npos
    );
}

TEST(RttiReader, RejectsNullVtable) {
    const rtti_reader reader{nullptr};
    EXPECT_EQ(reader.read(), std::nullopt);
}

} // namespace