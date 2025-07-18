#include <iostream>
#include <gtest/gtest.h>

#include "../experiments/experiment02.hpp"

TEST(SecondExperimentTest, CallsDerivedMethod) {
    experiment02::Derived derived{};
    experiment02::Base1* base1 = &derived;
    experiment02::Base2* base2 = &derived;
    
    EXPECT_EQ(base1->test(), 81);
    EXPECT_EQ(base2->test(), 81);
}