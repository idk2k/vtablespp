
# RTTI = "the object remembers what it really is"

Animal* a = new Cat();   // pointer type: Animal, real type: Cat
// a->???  — at runtime, how do I find out it's a Cat?

A base pointer only shows the static type. RTTI is the mechanism that stores the real (dynamic) type somewhere reachable at runtime.

Usage:
if (dynamic_cast<Cat*>(a))        // "is it really a Cat?" → safe downcast
    (dynamic_cast<Cat*>(a))->meow();

std::cout << typeid(*a).name();   // prints the real type name

Both only work on polymorphic classes (must have at least one virtual function) — because that's the only case where an object's real type differs from the pointer's type.

object → vtable ptr → vtable → [slot 0: pointer to TypeDescriptor("class Cat")]

The compiler, for every polymorphic class, adds one extra hidden entry to the vtable pointing at a type descriptor (a struct with the class name string). That's the whole trick — RTTI hangs off the vtable.

Cost: every polymorphic object carries a slightly heavier vtable (name strings, hierarchy info) and dynamic_cast walks the class hierarchy at runtime — that's why some build with it disabled (/GR- in MSVC, -fno-rtti in GCC/Clang). After that, dynamic_cast/typeid degrade or fail.

RTTI = compiler attaches a hidden "I am class X" tag (via the vtable) to polymorphic objects, so dynamic_cast and typeid can query the real type at runtime.

