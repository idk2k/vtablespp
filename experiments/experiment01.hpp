#include <iostream>

namespace experiment01 {
	class Base {
	public:
		Base(int ogo) : ogo(ogo) {}
		char opa{ 12 };
		int ogo{1};
		virtual int test() = 0;
	};

	class Derived : public Base {
	public:
		Derived(int ogo, int derive_ogo) : experiment01::Base{ ogo }, derive_ogo(derive_ogo) {}
		int derive_ogo{2};
		virtual int test() override {
			return 8;
		}
	};

	/*inline auto test() -> int {
		Derived derived_class{ 1, 2 };

		std::cout << "Derived address: " << (void*)&derived_class << std::endl;

		derived_class.test();
		int a; std::cin >> a;
		return 0;
	}*/
}