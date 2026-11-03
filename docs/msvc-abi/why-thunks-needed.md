# Why a return-adjustment thunk is needed

This document explains why MSVC uses a vftable thunk for covariant return
types, even though a `this` adjustment can sometimes be performed inside
the function body.

## 1. The question

In the `vtablespp` experiments we saw two different behaviors:

```cpp
struct Right {
    virtual Right* clone() const;
};

struct Derived : Left, Right {
    Derived* clone() const override;
};
```

For `this`, MSVC performed the adjustment inside the function body:

```asm
mov rax, qword ptr [this] ; Right*
sub rax, 8                ; Right* -> Derived*
```

So the natural question is:

```text
Why can we not adjust the returned pointer inside the function too?
Why do we need a thunk?
```

## 2. The short answer

```text
A this adjustment depends only on the static type of the caller.
The caller knows it at compile time.

A return adjustment depends on the dynamic type of the returned object.
The caller does not know it at compile time.

Therefore, the return adjustment must be bound to the vftable entry -
which is exactly what a thunk is.
```

## 3. The `this` case: adjustment is caller-dependent

For `clone()`, MSVC fixed one `this` contract:

```text
Derived::clone() receives Right* as this
```

Both call paths can be converted to this form before the call:

```text
r->clone()  -> rcx = Right*          (already correct)
d->clone()  -> Derived* -> Right*    (caller-side: add rax, 8)
```

The required adjustment depends only on the static type of the caller:

```text
Right* caller   -> adjustment: none
Derived* caller -> adjustment: +8
```

The compiler knows the static type at compile time, so it can emit the
conversion directly into the calling code:

```asm
; d->clone()
add rax, 8              ; Derived* -> Right*
mov rcx, rax
call qword ptr [rax+8]  ; direct entry
```

No runtime information is needed. The adjustment is fully determined by
the compile-time types.

## 4. The return case: adjustment is dynamic-type-dependent

Now look at the return value.

`Derived::clone()` naturally returns `Derived*`:

```cpp
return new Derived{*this};
```

But the two caller contracts expect different types:

```text
Right* caller   expects: Right*
Derived* caller expects: Derived*
```

At first glance, it seems the function could just always return `Derived*`,
and the `Right*` caller could adjust the result after the call:

```text
r->clone()
    -> returns Derived*
    -> caller adds 8
    -> Right*
```

Why is this not done?

## 5. The problem: the caller does not know the dynamic type

Consider a deeper hierarchy:

```cpp
struct Final : Derived {
    Final* clone() const override {
        return new Final{*this};
    }
};
```

Now:

```text
Final::clone() returns Final*
Derived::clone() returns Derived*
Right::clone() promises Right*
```

The offsets are different:

```text
Right*   = Derived* + 8
Right*   = Final* + 8      ; if layout is the same
```

But in general, the offset from the returned pointer to the `Right`
subobject depends on the dynamic type of the returned object.

The caller code looks like this:

```cpp
Right* r = &object;
Right* p = r->clone();
```

At compile time, the compiler knows only:

```text
r has type Right*
clone() promises Right*
```

It does not know whether the runtime object will be:

```text
Derived  -> returned Derived*
Final    -> returned Final*
```

Therefore, the caller cannot emit a fixed adjustment:

```text
add rax, ???
```

The required `???` depends on the dynamic type, which is known only at
runtime.

## 6. The vftable solves this

The vftable entry is selected together with the final overrider. So the
entry can bundle:

```text
1. Which implementation to call
2. Which adjustment its result needs
```

For the `Right*` contract:

```text
Right vftable slot[0]:
    Derived object  -> thunk: call Derived::clone, then +8
    Final object    -> thunk: call Final::clone, then +8 (or other offset)
```

Each vftable belongs to a specific dynamic type, so its thunk knows the
exact adjustment for that type.

```text
r->clone()
    -> runtime vftable of the actual object
    -> slot[0]
    -> thunk for THAT dynamic type
    -> correct adjustment
    -> Right*
```

This is why the adjustment must live in the vftable, not in the caller.

## 7. Why the direct entry needs no adjustment

For the `Derived*` contract:

```text
Derived vftable slot[1]:
    Derived::clone() -> returns Derived*
    caller expects Derived*
    -> no adjustment
```

But if a `Final` object overrides `clone()`:

```text
Final vftable slot[1]:
    Final::clone() returns Final*
    caller through Derived* expects Derived*
    -> adjustment needed: Final* -> Derived*
    -> this entry becomes a thunk too
```

So the "direct entry" is direct only while the implementation's natural
return type matches the caller contract. As soon as a further-derived
class changes the return type, that entry also becomes a thunk.

This shows the general rule:

```text
Each vftable entry pairs:
    the final overrider
with
    the adjustment required for its caller contract.
```

## 8. Why `this` is different

For `this`, the situation is reversed:

```text
The caller knows its own static type at compile time.
The required this form is fixed by the ABI (introducing class).
Therefore, the caller can adjust before the call.
```

```text
d->clone()
    compile-time known: Derived* caller
    fixed ABI form:     Right* this
    -> caller emits: add rax, 8
```

No runtime information is needed, because the adjustment depends only on
compile-time types.

For the return value:

```text
The caller knows only the promised return type (Right*).
The actual returned pointer type depends on the dynamic type.
Therefore, the caller cannot adjust.
```

The adjustment must be attached to the vftable entry, which is selected at
runtime together with the implementation.

## 9. Could the function adjust its own result?

In principle, `Derived::clone()` could always return `Right*`:

```cpp
// hypothetical
Right* clone() const override {
    return new Derived{*this}; // compiler adjusts inside
}
```

But then:

```text
d->clone() would receive Right*
and would need to convert it back to Derived*
```

This is possible for `Derived` itself, but it breaks down with further
derivations:

```text
Final::clone() returns Final*
If the function always returned Right*:
    Derived* caller would need Final* -> Derived*
    but the offset depends on the dynamic type
    -> same problem as before
```

The function body cannot know which caller contract will be used, and the
caller cannot know which dynamic type will be returned. The only place
where both pieces of information meet is the vftable entry.

## 10. The asymmetry summarized

```text
this:
    adjustment depends on caller static type
    -> known at compile time
    -> can be done at the call site or inside the function

return value:
    adjustment depends on dynamic type of the returned object
    -> known only at runtime
    -> must be bound to the vftable entry
    -> thunk
```

## 11. Verified example

From the experiment:

```text
Right vftable:
slot[0]: 00007FF74E103A10  -> thunk for clone()
slot[1]: 00007FF74E101850  -> direct Derived::clone()
```

Thunk:

```asm
call Derived::clone       ; returns Derived*
cmp qword ptr [rsp+20h], 0
je skip
add rax, 8                ; Derived* -> Right*
skip:
ret
```

Direct entry:

```asm
; Derived::clone body
; returns Derived* directly
```

Call sites:

```text
r->clone()  -> call [rax]      -> slot[0] -> thunk
d->clone()  -> call [rax+8]    -> slot[1] -> direct
```

## 12. Intuitive summary

Imagine a clone factory:

```text
Derived::clone() always builds a whole new house
and returns the address of the whole house (Derived*).
```

But visitors arrive through different entrances:

```text
Right* visitor expects: "Right entrance address"
Derived* visitor expects: "whole house address"
```

The factory cannot know which visitor will come. So each entrance has its
own receptionist:

```text
Right entrance receptionist (thunk):
    takes the whole-house address
    converts it to the Right entrance address
    hands it to the Right* visitor

Main entrance receptionist (direct entry):
    hands the whole-house address to the Derived* visitor
```

The receptionist is the thunk. It is bound to the entrance (vftable slot),
not to the factory (function body), because only the entrance knows which
contract the visitor expects.

And with deeper hierarchies, each new house type may need its own
receptionist with its own conversion - which is exactly why the adjustment
must live in the vftable, selected at runtime together with the
implementation.

## 13. Resources

- Raymond Chen, "Adjustor thunks" (The Old New Thing):
  https://devblogs.microsoft.com/oldnewthing/20040206-00/?p=40723/
- Microsoft Learn: The this pointer:
  https://learn.microsoft.com/en-us/cpp/cpp/this-pointer
- Microsoft Learn: Multiple Base Classes:
  https://learn.microsoft.com/en-us/cpp/cpp/multiple-base-classes
- Microsoft Learn: x64 calling convention:
  https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention
- cppreference: The this pointer:
  https://en.cppreference.com/w/cpp/language/this
- C++ working draft, virtual functions:
  https://eel.is/c++draft/class.virtual
- Itanium C++ ABI (contrast; different thunk model):
  https://itanium-cxx-abi.github.io/cxx-abi/
- Related project documents:
  - `docs/msvc-abi.md`
  - `docs/this-pointer-msvc-abi.md`
