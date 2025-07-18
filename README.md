# vtables reference guide

# MSVC

## 1. C++20 (/std:c++20)
1. Base <-- Derived
with "virtual test()" method;

2. (Base1, Base2) <-- Derived
with "virtual test()" method in both classes Base1, Base2. Override in Derived;

3. (Base1, Base2) <-- Derived
with virtual methods in Base1, Base1, and new virtual method in Derived (only in Derived meta()),
so MSVC construct vtable of Derived as (vtable(Base1) + vtable(Derived)). Concat this two tables
in beginning of the Derived class;
Base2 has own offset and own vtable, with test() implementation in it, without meta() method
from Derived class. 


