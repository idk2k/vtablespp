#include <iostream>
#include <gtest/gtest.h>

#include "../experiments/experiment03.hpp"

TEST(ThirdExperimentTest, CallsDerivedMethod) {
    experiment03::Derived derived{};
    experiment03::Base1* base1 = &derived;
    experiment03::Base2* base2 = &derived;
    experiment03::Derived* pDerived = &derived;

    EXPECT_EQ(base1->test(), 8);
    EXPECT_EQ(base2->test(), 8);
    EXPECT_EQ(pDerived->meta(), 12900);
}