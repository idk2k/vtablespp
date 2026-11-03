# MSVC ABI notes

This project studies implementation-defined details of the Microsoft Visual C++ ABI.
They are **not** guaranteed by the ISO C++ standard and may change between compiler
versions, architectures, and build flags.

## Core concepts

- A polymorphic object contains a pointer to its primary vftable.
- A class with multiple bases can have several vftables: one per polymorphic base subobject.
- New virtual functions introduced in a derived class are appended to the primary vtable.
- Overridden virtual functions replace corresponding entries.
- Virtual inheritance additionally uses vbtables, which are distinct from ordinary vftables.

## Important MSVC RTTI structures

- `RTTICompleteObjectLocator`
- `RTTITypeDescriptor`
- `RTTIClassHierarchyDescriptor`
- `RTTIBaseClassDescriptor`

MSVC typically places the Complete Object Locator immediately before the method-pointer
array referenced by the object's vftable pointer.

## Current scope

The initial implementation targets:

- MSVC
- C++20
- x64 first, then x86
- Single and multiple inheritance
- Later: virtual inheritance and GCC/Itanium ABI support

# MSVC x64 ABI: vftables, multiple inheritance, and covariant return thunks

Technical notes from the `vtablespp` experiments.

Target: Windows, MSVC x64, C++20, Debug build.

All addresses are real examples from debugger sessions. They differ between
runs and builds, but the structural conclusions were verified in the
debugger.

## 1. Scope

This document describes observed MSVC x64 behavior. It is not a portable
C++ specification.

- The library inspects vftables; it does not call extracted addresses.
- Slot counts are supplied explicitly by the caller.
- A vftable is not an isolated array: RTTI structures and neighboring
  vftables may be located next to it.
- A null entry does not reliably mean "end of vftable".

## 2. Fundamental model

A polymorphic object stores a hidden pointer to a vftable (`vfptr`) in each
polymorphic base subobject.

```text
object
    |
    | vfptr
    v
vftable
    |
    | slot[n]
    v
function code
```

On x64, a pointer occupies 8 bytes.

A virtual call has two independent parts:

```text
1. Which vftable and slot?  -> determined by the static type of the caller
2. Which implementation?    -> determined by the dynamic type of the object
```

The compiler emits a call with a fixed slot offset. It does not search the
vftable from the beginning.

## 3. Experimental hierarchy

```cpp
struct Left {
    virtual void left() const {
        std::cout << (void*)this << std::endl;
    }
};

struct Right {
    virtual Right* clone() const {
        return new Right{*this};
    }
};

struct Derived : Left, Right {
    void left() const override {
        std::cout << (void*)this << std::endl;
    }

    Derived* clone() const override {
        return new Derived{*this};
    }

    int x{20};
};
```

`Derived::clone()` overrides `Right::clone()` with a covariant return type:

```text
Right::clone()   promises: Right*
Derived::clone() returns:  Derived*
```

This is allowed because `Derived` inherits from `Right`.

## 4. Object layout

Observed addresses:

```text
Complete object / Left subobject:
    0000004CCD52F588

Right subobject:
    0000004CCD52F590
```

Therefore:

```text
Derived* = 0000004CCD52F588
Left*    = 0000004CCD52F588
Right*   = 0000004CCD52F590

Right* = Derived* + 8
```

Layout:

```text
Derived object
+------------------------------------+
| Left subobject      <- offset 0    |
|   vfptr -> Left vftable (0xD270)   |
+------------------------------------+
| Right subobject     <- offset 8    |
|   vfptr -> Right vftable (0xD288)  |
+------------------------------------+
| int x                              |
+------------------------------------+
```

`Left` is the primary base: its subobject is located at offset 0, and its
vftable serves calls made through `Derived*` and `Left*`.

## 5. Observed vftable layout

```text
Left vftable (0xD270):

slot[0]: 00007FF74E1016F0   Derived::left
slot[1]: 0000000000000000   unidentified boundary entry
slot[2]: 00007FF74E10E780   pointer to RTTI data

Right vftable (0xD288):

slot[0]: 00007FF74E103A10   thunk for clone()
slot[1]: 00007FF74E101850   Derived::clone()
slot[2]: 0000000000000000   boundary / end of observed region
```

The verified function entries are:

```text
Left vftable slot[0]  -> Derived::left
Right vftable slot[0] -> thunk for Derived::clone
Right vftable slot[1] -> Derived::clone
```

The entries after `slot[0]` of the Left vftable were read by inspecting
five consecutive slots. They cross the boundary of the Left vftable into
service data and the Right vftable.

## 6. RTTI near vftables

The pointer at `0xD280` points to RTTI metadata:

```text
0xD280: 00007FF74E10E780
```

Near that address, the debugger showed:

```text
Right::`RTTI Base Class Descriptor at (8,-1,0,64)'
```

MSVC places a pointer to an `RTTICompleteObjectLocator` immediately before
a vftable:

```text
vftable[-1] -> RTTI Complete Object Locator
vftable[0]  -> first virtual function entry
```

Here:

```text
0xD280 = RTTI Complete Object Locator pointer
0xD288 = Right vftable
```

So `0xD280` is `Right vftable[-1]`.

RTTI is used by `dynamic_cast`, `typeid`, and exception handling. It is not
part of the normal virtual-call slot array.

Disassembling `0xE780` produces meaningless instructions because it is
data, not code:

```asm
add byte ptr [rax], al
add dword ptr [rax], eax
```

This does not mean that such code exists. It means the disassembler is
interpreting data bytes as instructions.

## 7. Canonical form of a virtual function

The experiments support this MSVC rule:

```text
A virtual function is canonicalized to the form
of the class where it was first declared.
```

For `clone()`:

```text
Introducing class:      Right
Canonical this:         Right*
Canonical vftable:      Right vftable
```

Therefore, both call paths use the Right vftable:

```text
r->clone()  -> Right vftable
d->clone()  -> Right vftable
```

The difference is the caller contract and therefore the selected slot.

## 8. Call through `Right*`

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

Chain:

```text
r->clone()
    -> Right vftable slot[0]
    -> thunk
    -> Derived::clone()
    -> returns Derived*
    -> thunk adds 8
    -> returns Right*
```

The caller expects `Right*`, so the thunk must adjust the result:

```text
Derived* + 8 -> Right*
```

Thunk disassembly:

```asm
mov rax, qword ptr [this]      ; this = Right*
mov rcx, rax
call Derived::clone            ; returns Derived*

mov qword ptr [rsp+20h], rax
cmp qword ptr [rsp+20h], 0
je skip_adjustment

mov rax, qword ptr [rsp+20h]
add rax, 8                     ; Derived* -> Right*
mov qword ptr [rsp+28h], rax
jmp return

skip_adjustment:
mov qword ptr [rsp+28h], 0

return:
mov rax, qword ptr [rsp+28h]
add rsp, 30h
pop rdi
ret
```

The null check matters: if `clone()` returned `nullptr`, adding `8` would
produce an invalid pointer.

## 9. Call through `Derived*`

```cpp
Derived* d = &object;
d->clone();
```

Disassembly:

```asm
mov rax, qword ptr [d]         ; rax = Derived*
add rax, 8                     ; rax = Right*

mov rcx, qword ptr [d]         ; rcx = Derived*
mov rcx, qword ptr [rcx+8]     ; rcx = Right vfptr

mov qword ptr [rsp+0F8h], rcx
mov rcx, rax                   ; this = Right*

mov rax, qword ptr [rsp+0F8h]  ; rax = Right vftable
call qword ptr [rax+8]         ; Right vftable slot[1]
```

Chain:

```text
d->clone()
    -> Derived* is converted to Right*
    -> Right vftable slot[1]
    -> direct Derived::clone()
    -> returns Derived*
```

No return adjustment is needed, because the caller expects `Derived*`.

## 10. Why `this` is converted at the call site

MSVC compiles one implementation of `Derived::clone()` with a fixed
`this` form:

```text
Derived::clone(Right* this)
```

This is the introducing-class form. Therefore, a caller using `Derived*`
must convert its pointer before the call:

```text
Derived* -> Right*
```

This happens in the caller code, before control reaches the function:

```asm
add rax, 8
mov rcx, rax
```

Inside the function, when the implementation needs `Derived*` — for
example, to execute `new Derived{*this}` — it converts back:

```asm
mov rax, qword ptr [this] ; Right*
sub rax, 8                ; Right* -> Derived*
```

So:

```text
d->clone():
    Derived* -> Right*        ; caller-side adjustment
    -> Derived::clone(Right* this)
    -> inside: Right* -> Derived*
    -> creates Derived
    -> returns Derived*
```

The conversion at the call site is not performed because the implementation
"needs" `Derived*`. It is performed because MSVC fixed one `this` contract
for all call paths.

## 11. Why two entries for one function

`clone()` is one function, but it has two caller contracts:

```text
Right* caller:
    expects Right*
    needs return adjustment
    uses thunk

Derived* caller:
    expects Derived*
    needs no return adjustment
    uses direct entry
```

Therefore, the Right vftable contains two entries:

```text
slot[0] -> thunk       ; for Right* callers
slot[1] -> direct call ; for Derived* callers
```

This is not duplication of the function. It is two entry points for one
implementation, each satisfying a different static caller contract.

## 12. Why the result must be adjusted

The pointers point to different subobjects:

```text
Derived* = 0000004CCD52F588
Right*   = 0000004CCD52F590
```

If `Derived::clone()` returned its `Derived*` directly to a caller that
expects `Right*`, that caller would treat the beginning of the object as a
`Right` subobject.

But the real `Right` subobject is 8 bytes later:

```text
Derived* + 8 = Right*
```

A `Right*` must point to the `Right` subobject, because virtual calls
through it read the `vfptr` stored there.

Therefore:

```text
Derived* + 8 -> Right*
```

is required before returning to a `Right*` caller.

## 13. Why the direct entry is called, not the thunk

The compiler does not scan the vftable from the beginning. It selects the
slot at compile time based on the static type of the call:

```text
d->clone()
    static type: Derived*
    expected result: Derived*
    no return adjustment needed
    -> slot[1]
    -> call [rax + 8]

r->clone()
    static type: Right*
    expected result: Right*
    return adjustment needed
    -> slot[0]
    -> call [rax]
```

The slot index is a compile-time constant:

```text
slot[0] -> offset 0x00
slot[1] -> offset 0x08
slot[2] -> offset 0x10
slot[3] -> offset 0x18
slot[4] -> offset 0x20
```

The thunk in `slot[0]` is not "first by priority". It is simply the entry
assigned to the `Right*` caller contract.

## 14. Simple overrides without covariant return

For a function declared in `Right` and overridden in `Derived`:

```cpp
struct Right {
    virtual int right() const;
};

struct Derived : Left, Right {
    int right() const override;
};
```

The canonical `this` is still `Right*`.

```text
r->right()
    -> Right vftable
    -> direct Derived::right()
    -> this = Right*

d->right()
    -> Derived* -> Right*
    -> Right vftable
    -> direct Derived::right()
    -> this = Right*
```

Because `right()` returns `int`, there is no return pointer to adjust.
Therefore, no return-adjustment thunk is needed.

If the implementation needs `Derived*` internally, it converts `this`
inside its own body:

```asm
mov rax, qword ptr [this] ; Right*
sub rax, 8                ; Right* -> Derived*
```

So:

```text
No thunk does not mean no pointer adjustment.
The adjustment may happen inside the function body.
```

## 15. Where adjustments happen

There are three distinct places:

### 1. Caller-side adjustment

```text
Derived* -> Right*
```

Performed before the call, in the caller code:

```asm
add rax, 8
mov rcx, rax
call qword ptr [rax+8]
```

### 2. Adjustment inside the function body

```text
Right* -> Derived*
```

Performed when the implementation needs the derived form of `this`:

```asm
mov rax, qword ptr [this]
sub rax, 8
```

### 3. Vftable thunk

Used when the caller contract requires adaptation that cannot be performed
by a simple caller-side conversion:

```text
Right* caller
    -> thunk
    -> Derived::clone()
    -> Derived*
    -> +8 -> Right*
```

A thunk may adjust `this`, the return value, or both.

## 16. Table adjacency

In one dump:

```text
Left vftable:  00007FF74E10D270
Right vftable: 00007FF74E10D288
```

The difference is:

```text
0xD288 - 0xD270 = 0x18 = 3 slots
```

Therefore:

```text
Left vftable slot[3] = Right vftable slot[0]
Left vftable slot[4] = Right vftable slot[1]
```

MSVC stores vftables as adjacent regions in read-only data. A secondary
vftable may begin inside the same larger data region.

This is why reading many slots sequentially can cross the boundary between
one vftable and another.

## 17. Debugger workflow

### Show symbol names

1. Start debugging and break at a known point.
2. Open `Debug -> Windows -> Disassembly`.
3. Right-click inside the Disassembly window.
4. Enable `Show Symbol Names`.

### Check PDB symbols

1. Open `Debug -> Windows -> Modules`.
2. Find the executable.
3. Check `Symbol Status`.
4. If symbols are missing, right-click the module and choose
   `Load Symbols`.

For a Debug build, the PDB is usually next to the executable:

```text
build\msvc-x64-debug\examples\Debug\call_multiple_inheritance.pdb
```

### Identify a vftable slot

1. Put a breakpoint on the virtual call:

```cpp
r->clone();
```

2. When the debugger stops, open Disassembly.
3. Find the indirect call:

```asm
call qword ptr [rax]
```

4. Use `Step Into` (`F11`).
5. The debugger jumps to the target of that slot and shows its symbol.

### Recognize MSVC symbols

```text
??_7Derived@@6B@    vftable
??_R0...            TypeDescriptor
??_R1...            Base Class Descriptor
??_R3...            Class Hierarchy Descriptor
??_R4...            Complete Object Locator
```

## 18. Lessons for vtablespp

1. One virtual function may have several vftable entries.

2. Entries are selected by the static type of the caller.

3. A thunk adapts either `this`, the return value, or both.

4. RTTI structures live near vftables and must not be interpreted as
   function slots.

5. Vftable boundaries cannot be reliably discovered by scanning memory.

6. Therefore `vtable_view` requires an explicit, trusted slot count.

7. Extracted addresses should be displayed and compared with symbols,
   not invoked.

## 19. Intuitive summary

Imagine an object with multiple inheritance as a house with several
entrances:

```text
Derived object
+------------------------------------+
| Left entrance   <- offset 0        |
| Right entrance  <- offset 8        |
+------------------------------------+
```

Each entrance has its own signboard — a vftable — telling visitors which
functions are available through that entrance.

```text
Left entrance signboard:
    "left() is here"

Right entrance signboard:
    "clone() is here"
```

When you call:

```cpp
Right* r = &object;
r->clone();
```

you enter through the Right entrance. The signboard says: "clone is in
cell 0". But this entrance promises that `clone()` will give you back a
`Right*`. The actual implementation, `Derived::clone()`, naturally produces
a `Derived*` — the address of the whole new house. Since the Right entrance
is 8 bytes deeper than the main entrance, a small helper — the thunk — takes
the returned "whole house" address and converts it to "Right entrance"
address:

```text
Derived* + 8 -> Right*
```

When you call:

```cpp
Derived* d = &object;
d->clone();
```

you start at the main entrance, but the compiler knows that `clone()` was
first declared at the Right entrance. So it immediately walks 8 bytes to the
Right entrance and calls the direct implementation there. This time, the
caller expects a `Derived*` — the whole house — and the implementation
returns exactly that. No thunk is needed.

So the mental model is:

```text
The static type of the pointer chooses the entrance.

The entrance's vftable chooses the slot.

The dynamic type of the object chooses the implementation.

If the promised result differs from the natural result,
a thunk converts it.
```

In short:

```text
One object.
Several base subobjects.
Several vftables.

One virtual function may have several entries,
because different callers expect different contracts.

The compiler knows in advance which entry to use,
so a virtual call is always a jump to a fixed slot.

A thunk is a tiny adapter that makes the result
—or sometimes this—fit the caller's contract.
```
