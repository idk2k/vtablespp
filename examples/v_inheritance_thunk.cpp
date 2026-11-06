//
// VIRTUAL INHERITANCE return adjustment thunk
// It is more complex: first it looks to vbtable for find an actual base of Interface
//

struct Interface {
    virtual Interface* clone() const = 0;
};

struct Derived : virtual Interface {
    Derived* clone() const override { return new Derived(*this); }
};

int main() {
    Derived object{};
    Interface* r = &object;
    Interface* p = r->clone();

    return 0;
}