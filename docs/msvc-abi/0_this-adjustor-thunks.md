# This-Adjustor Thunks in MSVC x64

## What an adjustor thunk is

An **adjustor thunk** is a tiny compiler-generated function that corrects
the `this` pointer before transferring control to the real function.

```asm
[thunk]:Derived::left`adjustor{8}':
    sub rcx, 8          ; Right* -> Left* (start of Derived)
    jmp Derived::left
```

It exists because one virtual function implementation can be reachable
through several vftables, and each vftable passes its own form of `this`.

## The core rule

A this-adjustor thunk is generated when **one implementation** is the
final overrider for **several bases** whose vftables pass **different
forms of `this`**.

```text
one function + several vftables + different this forms
    => adjustor thunk is required
```

If each vftable has its own function, no thunk is needed: every function
already expects the form of `this` that its own vftable passes.

## When it appears

```cpp
struct Left {
    virtual int left() const { return 1; }
};

struct Right {
    virtual int left() const { return 2; }
};

struct Derived : Left, Right {
    virtual int left() const { return 30; }  // one override for both
};

int main() {
    Right* a = new Derived{};
    a->left();
}
```

Here `Derived::left` is the final overrider for **both** `Left` and
`Right`. The compiler fixes one canonical form of `this` (the start of
`Derived`, i.e. the `Left*` form) and generates a thunk for the second
vftable:

```text
Left vftable:  left -> Derived::left              this = Left*  (offset 0)
Right vftable: left -> [thunk]adjustor{8}         this = Right* (offset 8)
```

The thunk corrects the pointer before the call:

```asm
[thunk]:Derived::left`adjustor{8}':
    sub rcx, 8
    jmp Derived::left
```

## When it does NOT appear

### Case 1: each base has its own implementation

```cpp
struct Left {
    virtual int left() const { return 1; }
};

struct Right {
    virtual int left() const { return 2; }
};

struct Derived : Left, Right {
    virtual int own() const { return 30; }
};
```

`Derived` does not override `left`. Each vftable points to its own base
implementation:

```text
Left vftable:  left -> Left::left    this = Left*   (matches)
Right vftable: left -> Right::left   this = Right*  (matches)
```

No conflict, no thunk.

### Case 2: the function is introduced in only one base

```cpp
struct IFirst {
    virtual void Foo() = 0;
};

struct ISecond {
    virtual void Bar() = 0;
};

struct Impl : IFirst, ISecond {
    void Foo() override;
    void Bar() override;
};
```

`Bar` is declared only in `ISecond`. Every virtual call to `Bar` goes
through an `ISecond*` vftable, so the form of `this` is always the same.
MSVC points the `ISecond` vftable directly at `Impl::Bar`:

```asm
const Impl::`vftable'{for `ISecond'}
    DQ FLAT:virtual void Impl::Bar(void)   ; direct, no thunk
```

## Why the caller cannot do the adjustment

For a virtual call, the caller knows only the static type:

```cpp
Right* a = ...;
a->left();
```

The caller does not know the dynamic type of `*a`. The required
correction depends on which vftable — and therefore which final
overrider — is selected at runtime. Only the vftable entry knows both:

- which function will run;
- which form of `this` that function expects.

That is why the correction must live in the vftable entry itself, in the
form of a thunk.

## Verified MSVC x64 output

For the `Left` / `Right` / `Derived` example with a single override:

```asm
const Impl::`vftable'{for `Left'}
    DQ FLAT:virtual int Derived::left(void)

const Impl::`vftable'{for `Right'}
    DQ FLAT:[thunk]:virtual int Derived::left`adjustor{8}' (void)
```

And the thunk itself:

```asm
[thunk]:Derived::left`adjustor{8}' PROC
    sub rcx, 8
    jmp Derived::left
[thunk]:Derived::left`adjustor{8}' ENDP
```

## Summary table

| Situation | Functions behind vftables | Forms of `this` | Thunk |
|---|---|---|---|
| Override in derived, function declared in both bases | One shared function | Two different forms | Yes |
| No override, each base keeps its own function | Two different functions | Each matches its own vftable | No |
| Function declared only in the second base | One function, one vftable path | One form only | No |
