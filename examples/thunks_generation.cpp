struct IA {
    virtual void Q() = 0;
};

struct IB {
    virtual void Q() = 0;
};

struct Impl : IA, IB {
    void Q() override{};
};

int main() {
    IB* a = new Impl{};
    a->Q();
    return 0;
}