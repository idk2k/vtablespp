#include <iostream>

struct Left {
    virtual void left() const { std::cout << "Left\n"; }
};

struct Right {
    virtual void print() const { std::cout << "Right\n"; }
};

struct Derived : Left, Right {
    void print() const override {
        std::cout << "Derived\n";
    }
};

int main() {
    Derived object{};
    
    Right* r = &object;
    r->print();

    Derived* d = &object;
    d->print();
    return 0;
}
