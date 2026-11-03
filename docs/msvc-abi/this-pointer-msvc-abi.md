# The nature of `this` in MSVC x64: C++ type vs ABI form

This document explains the two different "levels" of the `this` pointer
that appear when working with virtual functions, multiple inheritance, and
covariant return types on Windows / MSVC x64.

It is based on verified experiments from the `vtablespp` project.

## 1. The key idea

There are two different notions of `this`:

```text
1. C++ level:   the type of this inside the function body
2. ABI level:   the pointer form physically passed in rcx
```

They do not always match.

```text
C++ level:   this is always cv T*, where T is the class
             in which the member function is declared.

ABI level:   MSVC may pass this in the form of the
             introducing class - the class where the
             virtual function was first declared.
```

The compiler inserts conversions between these two forms automatically.

## 2. C++ level: the declared type of `this`

Inside a non-static member function of a class `T`, `this` has type:

```text
cv T*
```

For example:

```cpp
struct Derived : Left, Right {
    Derived* clone() const override {
        // here: this has type const Derived*
    }
};
```

The function `clone()` is declared inside `Derived`, so from the C++
language point of view:

```text
this : const Derived*
```

This is why the following code compiles without any explicit cast:

```cpp
std::cout << static_cast<const void*>(this) << '\n';
return new Derived{*this};
```

Both expressions require `const Derived*`, and that is exactly the type
of `this` inside the function body.

The C++ standard defines `this` as the address of the implicit object
parameter of the member function. Its type is determined by the class in
which the member function is declared, together with the cv-qualifiers of
the function. [Microsoft Learn: The this pointer; cppreference: The this
pointer]

## 3. ABI level: what MSVC physically passes

For a virtual function, MSVC passes `this` in a canonical form determined
by the introducing class - the class where the virtual function was first
declared.

```cpp
struct Right {
    virtual Right* clone() const;
};

struct Derived : Left, Right {
    Derived* clone() const override;
};
```

Here:

```text
Introducing class:      Right
ABI form of this:       Right*
C++ type inside body:   const Derived*
```

So at the machine level, `Derived::clone()` receives `Right*` in `rcx`,
even though the C++ type of `this` inside its body is `const Derived*`.

## 4. The verified experiment

```cpp
struct Left {
    virtual void left() const;
};

struct Right {
    virtual Right* clone() const {
        return new Right{*this};
    }
};

struct Derived : Left, Right {
    Derived* clone() const override {
        std::cout << static_cast<const void*>(this) << '\n';
        return new Derived{*this};
    }
};
```

Observed object layout:

```text
Derived* = 0000004CCD52F588   Left subobject, offset 0
Right*   = 0000004CCD52F590   Right subobject, offset 8

Right* = Derived* + 8
```

### Call through `Derived*`

```cpp
Derived* d = &object;
d->clone();
```

Disassembly:

```asm
mov rax, qword ptr [d]         ; rax = Derived*
add rax, 8                     ; rax = Right*    (ABI form)

mov rcx, qword ptr [d]         ; rcx = Derived*
mov rcx, qword ptr [rcx+8]     ; rcx = Right vfptr

mov qword ptr [rsp+0F8h], rcx
mov rcx, rax                   ; this = Right*  (ABI form)

mov rax, qword ptr [rsp+0F8h]  ; rax = Right vftable
call qword ptr [rax+8]         ; Right vftable slot[1]
```

Inside `Derived::clone()`, the first use of `this` is compiled as:

```asm
mov rax, qword ptr [this] ; ABI form: Right*
sub rax, 8                ; -> C++ form: const Derived*
```

Then:

```cpp
std::cout << static_cast<const void*>(this);
```

prints the `Derived*` address, not the `Right*` address.

### Call through `Right*`

```cpp
Right* r = &object;
r->clone();
```

Disassembly:

```asm
mov rax, qword ptr [r]     ; rax = Right*
mov rax, qword ptr [rax]   ; rax = Right vftable
mov rcx, qword ptr [r]     ; rcx = this = Right*
call qword ptr [rax]       ; Right vftable slot[0]
```

This path uses the thunk in `slot[0]`, because the caller expects the
result to be `Right*`.

## 5. Why the compiler adjusts inside the function

The ABI fixes the input format:

```text
Derived::clone() receives Right* as this
```

But the function body uses expressions that require `Derived*`:

```cpp
return new Derived{*this};
```

Here `*this` must be a `const Derived&`, because the copy constructor of
`Derived` expects it.

So the compiler inserts the conversion at the point of use:

```text
Right* -> Derived*
```

This is an ordinary implicit conversion required by the expression, not a
change of the ABI contract.

If the function body did not need `Derived*`, for example:

```cpp
Derived* clone() const override {
    return nullptr;
}
```

then the internal `sub rax, 8` would not appear.

## 6. The three places where adjustment happens

### 1. Caller-side adjustment

When the caller's static type differs from the introducing class:

```text
Derived* -> Right*
```

Performed before the call:

```asm
add rax, 8
mov rcx, rax
call qword ptr [rax+8]
```

### 2. Adjustment inside the function body

When the implementation needs the derived form of `this`:

```text
Right* -> Derived*
```

Performed at the first use of `this` inside the function:

```asm
mov rax, qword ptr [this]
sub rax, 8
```

### 3. Vftable thunk

When the caller contract requires adaptation that cannot be performed by a
simple caller-side conversion - for example, a covariant return:

```text
Right* caller
    -> thunk
    -> Derived::clone()
    -> Derived*
    -> +8 -> Right*
```

A thunk may adjust `this`, the return value, or both.

## 7. Covariant return and two vftable entries

`Derived::clone()` returns `Derived*`, while `Right::clone()` promises
`Right*`. The pointers point to different subobjects:

```text
Derived* = address of the complete object
Right*   = address of the complete object + 8
```

Therefore, the Right vftable contains two entries for `clone()`:

```text
slot[0] -> thunk
    for Right* callers
    contract: return Right*
    calls Derived::clone(), then adds 8

slot[1] -> direct Derived::clone()
    for Derived* callers
    contract: return Derived*
    no adjustment needed
```

The compiler selects the slot at compile time, based on the static type of
the caller:

```text
d->clone()  -> slot[1] -> call [rax + 8]
r->clone()  -> slot[0] -> call [rax]
```

The thunk is not "first by priority". It is simply the entry assigned to
the `Right*` caller contract.

## 8. Simple overrides without covariant return

For a function declared in `Right` and overridden in `Derived`:

```cpp
struct Right {
    virtual int right() const;
};

struct Derived : Left, Right {
    int right() const override;
};
```

The canonical `this` is still `Right*`:

```text
r->right()  -> Right* -> Right vftable -> direct Derived::right()
d->right()  -> Derived* -> Right* -> Right vftable -> direct
```

Because `right()` returns `int`, there is no return pointer to adjust, so
no return-adjustment thunk is needed.

If the implementation needs `Derived*` internally, the same inside-function
conversion appears:

```asm
mov rax, qword ptr [this] ; Right*
sub rax, 8                -> Derived*
```

## 9. Mental model

```text
The C++ type of this:
    determined by the class where the function is declared.

The ABI form of this:
    determined by the introducing class of the virtual function.

The compiler bridges the two forms automatically:
    at the call site, and/or
    inside the function body, and/or
    through a vftable thunk.
```

For `clone()`:

```text
C++ level:   this : const Derived*
ABI level:   rcx : Right*
Bridge:      sub rax, 8 inside the function
```

## 10. Resources

### Official Microsoft documentation

- The this pointer:
  https://learn.microsoft.com/en-us/cpp/cpp/this-pointer
- Multiple Base Classes:
  https://learn.microsoft.com/en-us/cpp/cpp/multiple-base-classes
- x64 calling convention:
  https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention
- Symbol / PDB files:
  https://learn.microsoft.com/en-us/cpp/build/walkthrough-build-and-debug-a-c-project-cmake

### Standard and reference material

- cppreference: The this pointer:
  https://en.cppreference.com/w/cpp/language/this
- cppreference: Implicit conversions:
  https://en.cppreference.com/w/cpp/language/implicit_cast
- C++ working draft, virtual functions:
  https://eel.is/c++draft/class.virtual

### ABI and thunk material

- Raymond Chen, "Adjustor thunks" (The Old New Thing):
  https://devblogs.microsoft.com/oldnewthing/20040206-00/?p=40723/
- Itanium C++ ABI (useful contrast; MSVC uses a different layout):
  https://itanium-cxx-abi.github.io/cxx-abi/
- MSVC RTTI structure overview:
  https://www.openrce.org/articles/full_view/23

### Project context

- `vtablespp` repository:
  https://github.com/idk2k/vtablespp
- Related project document: `docs/msvc-abi.md`
