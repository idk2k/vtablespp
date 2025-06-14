
# MSVC Layout of Virtual Tables (C++)


## - typical layout
----- Class Definitions ----
class X{
    O a;
    H b;
    virtual void c() = 0;
    virtual void d() = 0;
};

class Y : public X{
    virtual void c();
    virtual void d();
};
--------- Layout -----------

1>  class X size(24):
1>      +---
1>   0  | {vfptr}
1>   8  | a
1>  16  | b
1>      +---
1>  
1>  X::$vftable@:
1>      | &X_meta
1>      |  0
1>   0  | &X::c
1>   1  | &X::d
1>  
1>  X::c this adjustor: 0
1>  X::d this adjustor: 0
1>  
1>  
1>  class Y size(24):
1>      +---
1>      | +--- (base class X)
1>   0  | | {vfptr}
1>   8  | | a
1>  16  | | b
1>      | +---
1>      +---
1>  
1>  Y::$vftable@:
1>      | &Y_meta
1>      |  0
1>   0  | &Y::c
1>   1  | &Y::d
1>  
1>  Y::c this adjustor: 0
1>  Y::d this adjustor: 0
1>  
1>  

----------------------------

## - MSVC this adjustor thunks


- https://stackoverflow.com/questions/3128295/questions-about-this-pointer-adjustor-in-c-object-layout
- https://devblogs.microsoft.com/oldnewthing/20040206-00/?p=40723


## - type info

## - display vtable when debugging

- https://www.codeproject.com/Tips/90875/Displaying-vtable-when-debugging

## - "C++: under the hood"
Jan Gray, 1994.
Worked on MSVC compiler.

The Microsoft C++ Object Model described herein was originally designed by Martin O’Riordan and David Jones.


- https://www.openrce.org/articles/files/jangrayhood.pdf

## - vtordisp pragma

- https://learn.microsoft.com/ru-ru/cpp/preprocessor/vtordisp?view=msvc-170