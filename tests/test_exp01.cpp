#include <iostream>
#include <gtest/gtest.h>

int add(int a, int b) { return a + b; }

TEST(AdditionTest, SimpleAddition) {
    EXPECT_EQ(add(2, 3), 5);
    ASSERT_NE(add(2, 2), 5);
}