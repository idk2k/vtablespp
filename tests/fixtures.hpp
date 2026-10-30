#pragma once

#include <cstdint>

namespace vtablespp_test {

struct SingleBase {
    virtual ~SingleBase() = default;
    virtual int test() const { return 1; }
};

struct SingleDerived final : SingleBase {
    int test() const override { return 2; }
};

struct MultiBase1 {
    virtual ~MultiBase1() = default;
    virtual int test() const { return 1; }
};

struct MultiBase2 {
    virtual ~MultiBase2() = default;
    virtual int test() const { return 2; }
};

struct MultiDerived final : MultiBase1, MultiBase2 {
    int test() const override { return 3; }
    virtual int meta() const { return 42; }
};

} // namespace vtablespp_test