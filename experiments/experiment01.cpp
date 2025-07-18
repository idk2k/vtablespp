#include <iostream>

class Base {
public:
	Base(int ogo) : ogo(ogo) {}
	char opa{ 12 };
	int ogo;
	virtual int test() = 0;
};

class Derived : public Base {
public:
	Derived(int ogo, int derive_ogo) : Base{ ogo }, derive_ogo(derive_ogo) {}
	int derive_ogo;
	virtual int test() override {
		return 8;
	}
};

auto main() -> int {
	Derived derived_class{ 1, 2 };

	std::cout << "Derived address: " << (void*)&derived_class << std::endl;

	derived_class.test();
	int a; std::cin >> a;
	return 0;
}