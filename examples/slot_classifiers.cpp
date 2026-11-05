#include <iostream>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

#include <vtablespp/vtablespp.hpp>
#include <vtablespp/core/slot_classifier.hpp>

using namespace vtablespp;

struct Left {
    virtual int left() const {
        return 1;
    }
};

struct Right {
    virtual int right() const {
        return 2;
    }
};

struct Derived : Left, Right {
    int left() const override {
        return 10;
    }

    int right() const override {
        return 20;
    }

    virtual int own() const {
        return 30;
    }
};

int main() {
    Derived obj{};
    vtable_view view = vtable_view::from_object(obj, 8);
    slot_classifier classifier(view);

    slot_info info = classifier.inspect(3);
    return 0;
}