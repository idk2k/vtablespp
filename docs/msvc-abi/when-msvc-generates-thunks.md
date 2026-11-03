# When MSVC Generates Thunks

A practical summary of every thunk case explored in this research.

## 1. This-adjustor thunk

**When:** one virtual function implementation is the final overrider for
several bases, and those bases pass different forms of `this`.

```cpp
struct IA { virtual void Q() = 0; };
struct IB { virtual void Q() = 0; };

struct Impl : IA, IB {
    void Q() override;
};
```

```text
IA vftable:  Q -> Impl::Q                    this = IA* (offset 0)
IB vftable:  Q -> [thunk]adjustor{8}         this = IB* (offset 8)
```

The thunk corrects `this` before the call:

```asm
[thunk]:Impl::Q`adjustor{8}':
    sub rcx, 8
    jmp Impl::Q
```

**Key condition:** the same function must be reachable through several
vftables with different `this` forms.

## 2. No this-adjustor thunk: separate implementations

**When:** each base keeps its own implementation, and the derived class
does not override the function.

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

```text
Left vftable:  left -> Left::left     this = Left*   (matches)
Right vftable: left -> Right::left    this = Right*  (matches)
```

No shared function, no conflicting `this` forms, no thunk.

## 3. No this-adjustor thunk: function introduced in one base only

**When:** a virtual function is declared in only one base and overridden
in the derived class.

```cpp
struct IFirst {
    virtual void Foo() = 0;
};

struct ISecond {
    virtual void Bar() = 0;
};

struct Impl : IFirst, ISecond {
    void Bar() override;
};
```

All virtual calls to `Bar` go through the `ISecond` vftable, so the form
of `this` is always the same:

```text
ISecond vftable: Bar -> Impl::Bar   (direct, no thunk)
```

## 4. Return-adjustor / covariant-return thunk

**When:** a virtual function has a covariant return type, and the
implementation returns a more derived pointer than the introducing base
promises.

```cpp
struct Right {
    virtual Right* clone() const = 0;
};

struct Derived : Right {
    Derived* clone() const override;   // covariant return
};
```

The caller may accept the result as `Right*`, but the implementation
returns `Derived*`. The correction depends on the dynamic type of the
returned object, so it cannot be performed by the caller. It is bound to
the vftable entry:

```text
caller -> thunk: call Derived::clone -> convert Derived* to Right* -> caller
```

**Key difference from this-adjustor:**

```text
this-adjustor:   corrects this BEFORE the call
return-adjustor: corrects the result AFTER the call
```

## 5. When no thunk is needed at all

- The function is introduced in one base only, and all virtual dispatch
  paths pass the same form of `this`.
- Each vftable points to its own base implementation, so every function
  already receives the `this` form it expects.
- The call is devirtualized: the compiler knows the dynamic type and
  calls the final overrider directly.
- The caller accepts the natural return type of the implementation, so
  no return conversion is required.

## Summary

| Thunk type | Corrects | When generated |
|---|---|---|
| This-adjustor | `this` before the call | One implementation, several vftables, different `this` forms |
| Return-adjustor (covariant return) | Returned pointer after the call | Covariant return type, caller expects the base form |
| No thunk | — | One vftable path, matching `this` form, direct return type, or devirtualized call |

- https://godbolt.org/z/rzGve8E57
