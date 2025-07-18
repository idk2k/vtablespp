#include <iostream>
#include <gtest/gtest.h>

#include "../experiments/experiment01.hpp"

TEST(FirstExperimentTest, CallsDerivedMethod) {
    experiment01::Derived derived{ 1231,2123 };
    experiment01::Base* base = &derived;
    EXPECT_EQ(base->test(), 8);
}