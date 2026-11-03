# Vftable slot selection and thunk logic

This document captures the final and most important insight from the
`vtablespp` experiments: how the compiler selects between a direct entry
and a thunk, and why a thunk is sometimes not called at all.

It complements:

- `docs/msvc-abi.md`
- `docs/this-pointer-msvc-abi.md`
- `docs/why-thunk-needed.md`

## 0. INTUITION

When clone() is called, its introducing class is Right. This means the this pointer must be unambiguously converted to Right* before the call. Inside the function, everything depends on this form: if the implementation needs a Derived*, the compiler inserts a static adjustment (Right* → Derived*) at the point where it is used. This adjustment is fully static, because the offset is known at compile time; it never needs to be dynamic.

The return value, however, is different. The calling code cannot adjust the returned pointer itself, because it expects the type that was already fixed by its own static type: a Right* caller expects Right*, while a Derived* caller expects Derived*. More importantly, the caller does not know which implementation will actually run. Depending on the dynamic type of the object, the call may execute Derived::clone(), which returns a Derived*, or an override in a further-derived class, which returns a different pointer type with a different offset to the Right subobject. Since the required adjustment depends on the dynamic type of the returned object, the caller cannot know at compile time which conversion to apply.

The only place where both pieces of information meet is the vftable entry. The thunk is selected together with the vftable — that is, together with the dynamic type of the object — so it knows exactly which implementation will run and which adjustment its result requires. It calls the implementation and converts the returned pointer to the type promised by the caller's contract.

If the result is accepted as a Derived*, the thunk is not called at all. In that case, the compiler selects the direct entry in the vftable, because the natural result of the implementation — a Derived* — already matches the caller's contract, and there is nothing to adjust.

## 1. The central rule

```text
The compiler selects the vftable slot at compile time,
based on the static type of the caller.

The slot determines the contract:

    what this form is expected
    what return type is promised

The dynamic type of the object determines
which implementation will run.
```

A virtual call never "searches" the vftable from the beginning. It always
uses a fixed, compile-time-known slot offset.

## 2. The two entries for clone()

```cpp
struct Right {
    virtual Right* clone() const;
};

struct Derived : Left, Right {
    Derived* clone() const override;
};
```

The Right vftable contains two entries for `clone()`:

```text
slot[0] -> thunk
    contract: caller expects Right*
    calls Derived::clone()
    adjusts the result: Derived* + 8 -> Right*

slot[1] -> direct Derived::clone()
    contract: caller expects Derived*
    no adjustment needed
```

## 3. Call through `Right*`

```cpp
Right* r = &object;
r->clone();
```

```asm
mov rax, qword ptr [r]     ; Right*
mov rax, qword ptr [rax]   ; Right vftable
mov rcx, qword ptr [r]     ; this = Right*
call qword ptr [rax]       ; slot[0] -> thunk
```

Chain:

```text
r->clone()
    -> slot[0]
    -> thunk
    -> Derived::clone()
    -> Derived*
    -> +8
    -> Right*
```

The caller expects `Right*`, so the return value must be adjusted.
The thunk performs this adjustment.

## 4. Call through `Derived*`

```cpp
Derived* d = &object;
d->clone();
```

```asm
mov rax, qword ptr [d]         ; Derived*
add rax, 8                     ; Derived* -> Right* (this form)

mov rcx, qword ptr [d]
mov rcx, qword ptr [rcx+8]     ; Right vfptr

mov qword ptr [rsp+0F8h], rcx
mov rcx, rax                   ; this = Right*

mov rax, qword ptr [rsp+0F8h]  ; Right vftable
call qword ptr [rax+8]         ; slot[1] -> direct entry
```

Chain:

```text
d->clone()
    -> Derived* converted to Right* (this form)
    -> slot[1]
    -> direct Derived::clone()
    -> returns Derived*
```

The caller expects `Derived*`. The implementation naturally returns
`Derived*`. Nothing needs to be adjusted, so the thunk is not called at
all.

## 5. The key insight

```text
It is not that the thunk is called and skips the adjustment.

It is that a different slot is selected:
    one without any adjustment.
```

The compiler knows in advance:

```text
d->clone()  -> needs Derived*  -> slot[1] -> direct entry
r->clone()  -> needs Right*    -> slot[0] -> thunk
```

Therefore it emits different fixed offsets:

```asm
call qword ptr [rax+8]   ; d->clone()  -> slot[1]
call qword ptr [rax]     ; r->clone()  -> slot[0]
```

## 6. Why the compiler can choose in advance

The choice depends only on the static type of the caller:

```text
Right* caller
    promised result: Right*
    adjustment needed: yes
    -> thunk slot

Derived* caller
    promised result: Derived*
    adjustment needed: no
    -> direct slot
```

Both pieces of information are known at compile time:

```text
1. The static type of the pointer/reference
2. The declared return type of the function
```

So the compiler emits a fixed slot offset. The only runtime decision is
which vftable (and therefore which implementation) will be used.

## 7. What stays dynamic

```text
Static (compile time):
    which vftable slot to use
    which contract applies
    whether an adjustment is needed

Dynamic (runtime):
    which vftable is actually used
    which final overrider will run
```

For example, with a deeper hierarchy:

```cpp
struct Final : Derived {
    Final* clone() const override;
};
```

```text
r->clone() on a Final object:
    -> Final vftable slot[0]
    -> thunk for Final::clone()
    -> adjustment appropriate for Final

d->clone() on a Final object:
    -> Final vftable slot[1]
    -> entry for the Derived* contract
    -> may itself become a thunk if adjustment is needed
```

The slot indices stay fixed; their contents change per dynamic type.

## 8. The asymmetry with `this`

```text
this adjustment:
    depends on the static type of the caller
    -> known at compile time
    -> done at the call site (Derived* -> Right*)

return adjustment:
    depends on the dynamic type of the returned object
    -> known only at runtime
    -> must live in the vftable entry (thunk)
```

This is why:

```text
d->clone() converts this at the call site,
but needs no return adjustment.

r->clone() needs no this conversion,
but needs a return-adjustment thunk.
```

## 9. When is a thunk called?

```text
A thunk is called only when the caller contract
requires an adjustment.

If the caller contract matches the natural result
of the implementation, the direct entry is used,
and the thunk is not involved at all.
```

```text
Right* caller:
    promised Right*
    natural result Derived*
    mismatch -> thunk

Derived* caller:
    promised Derived*
    natural result Derived*
    match -> direct entry
```

## 10. Intuitive summary

Imagine the Right vftable as a reception desk with two windows:

```text
Window 0 (thunk):
    "You came through the Right entrance.
     You expect a Right entrance address.
     I will call the factory and convert
     the whole-house address for you."

Window 1 (direct):
    "You came as a Derived* visitor.
     You expect the whole-house address.
     The factory already returns exactly that.
     Take it directly."
```

The compiler knows in advance which window each caller will use:

```text
Right* visitor   -> window 0
Derived* visitor -> window 1
```

So it emits a fixed jump to the correct window. The visitor never walks
from window 0 to window 1, and the factory never needs to guess who is
asking.

## 11. One-sentence summary

```text
The vftable slot is selected by the static caller contract,
the implementation by the dynamic type,
and a thunk exists only for the contracts
where the promised result differs from the natural one.
```
