#include <iostream>

class Base1 {
public:
	//Base(int ogo) : ogo(ogo) {}
	int ogo{ 1 };
	virtual int test() = 0;
};

class Base2 {
public:
	//Base(int ogo) : ogo(ogo) {}
	int ogo2{ 2 };
	virtual int test() = 0;
};

class Derived : public Base1, public Base2 {
public:
	//Derived(int ogo, int derive_ogo) : Base{ogo}, derive_ogo(derive_ogo) {}
	int derived_ogo{ 3 };

	virtual int test() override {
		return 8;
	}

	virtual int meta() = 0;
};

class Gamma : public Derived {
public:
	virtual int meta() override {
		return 1234;
	}
};


auto main() -> int {
	//Derived derived_class{};
	Gamma gamma{};

	/* Gamma method meta() call */
	gamma.meta();

	Derived* der = &gamma;
	der->meta();

	//std::cout << "Derived address: " << (void*)&derived_class << std::endl;

	//derived_class.test();

	//Base2* bs2 = &derived_class;
	//bs2->test();

	//Base1* bs1 = &derived_class;
	//bs1->test();

	//derived_class.meta();

	

	int a; std::cin >> a;
	return 0;
}