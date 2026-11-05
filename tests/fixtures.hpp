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

// struct MultiBase1 {
//     virtual ~MultiBase1() = default;
//     virtual int test() const { return 1; }
// };

// struct MultiBase2 {
//     virtual ~MultiBase2() = default;
//     virtual int test() const { return 2; }
// };

// struct MultiDerived final : MultiBase1, MultiBase2 {
//     int test() const override { return 3; }
//     virtual int meta() const { return 42; }
// };

struct Left {
    virtual int left() const { return 1; }
};

struct Right {
    virtual int right() const { return 2; }
};

struct MultiDerived : Left, Right {
    int left() const override { return 10; }
    int right() const override { return 20; }
};

struct LeftThunk {
    virtual void Q() = 0;
    virtual ~LeftThunk() = default;
};

struct RightThunk {
    virtual void Q() = 0;
    virtual ~RightThunk() = default;
};

struct MultiDerivedThunk final : LeftThunk, RightThunk {
    void Q() override {
        int x = 2;
        x = 2 + 1;
        // Одна реализация является final overrider
        // и для Left::Q, и для Right::Q.
    }
};

struct SharedLeft {
    virtual void Q() = 0;
    virtual ~SharedLeft() = default;
};

struct SharedRight {
    virtual void Q() = 0;
    virtual ~SharedRight() = default;
};

struct SharedOverrideDerived final
    : SharedLeft,
      SharedRight {
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

struct SeparateImplementationsDerived final
    : SeparateLeft,
      SeparateRight {};

} // namespace vtablespp_test