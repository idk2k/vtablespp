#include <iostream>
#include <gtest/gtest.h>

class Base1 {
public:
	int ogo{ 1 };
	virtual int test() = 0;
};

class Base2 {
public:
	int ogo2{ 2 };
	virtual int test() = 0;
};

class Derived : public Base1, public Base2 {
public:
	int derived_ogo{ 3 };

	virtual int test() override {
		return 8;
	}

	virtual int meta() {
		return 12900;
	}
};

//class Gamma : public Derived {
//public:
//	virtual int meta() override {
//		return 1234;
//	}
//};

auto main(int argc, char **argv) -> int {
	// 
	Derived der{};
	Derived* p_der = &der;
	p_der->meta();
	der.meta();
	der.test();

	// Run tests
	testing::InitGoogleTest(&argc, argv);
	int res = RUN_ALL_TESTS();
	int a; std::cin >> a;
	return 0;
}