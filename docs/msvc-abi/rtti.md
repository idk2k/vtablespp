
# RTTI - Run-Time Type Information
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

## RTTICompleteObjectLocator

The vfptr in the object points to vftable[0] (first function pointer). One pointer-width before that, MSVC stores a pointer to the RTTICompleteObjectLocator:

void* vftable = *(void**)obj;      // object → vfptr → vftable[0]
void* col    = ((void**)vftable)[-1];   // ← the -1 slot

Cat object (x64)                Cat::`vftable'
┌──────────────┐               ┌─────────────────────────┐
│ vfptr ───────┼──────────────►│ [-1] → COL              │ ◄─ 8 bytes before [0]
├──────────────┤               │ [ 0] deleting dtor      │
│ int age      │               │ [ 1] Cat::Speak         │
└──────────────┘               │ [ 2] ...                │
                               └─────────────────────────┘
Two caveats:

Only when RTTI is enabled (/GR, default on) and the class is polymorphic.
GCC/Clang (Itanium ABI) do it differently: [-1] = pointer to typeinfo, [-2] = offset-to-top. So "-1 slot" is the MSVC convention

2) The MSVC RTTI structure family (x86 first)

Everything hangs off the COL — it's the root node:

RTTICompleteObjectLocator (COL)
├── signature          // 0x00000000 on x86
├── offset             // where THIS vftable sits inside the complete object
├── cdOffset           // constructor displacement (virtual inheritance; usually 0)
├── pTypeDescriptor ──────►  TypeDescriptor { type_info vftable, spare, ".?AVCat@@" }
└── pClassDescriptor ─────►  ClassHierarchyDescriptor
                              ├── attributes        // 0x1 = multiple inheritance, 0x2 = virtual inheritance
                              ├── numBaseClasses    // count incl. itself
                              └── pBaseClassArray ──► [ BCD_Cat ][ BCD_Animal ][ BCD_IThing ]
                                                        BaseClassDescriptor:
                                                        { pTypeDescriptor, numContainedBases,
                                                          PMD { mdisp, pdisp, vdisp }, attributes }
What each piece is for:

offset — matters with multiple inheritance. struct C : A, B {} produces two vftables in the object; the primary COL has offset = 0, the secondary (for the B subobject) has offset = sizeof(A). That's how the runtime knows "this vftable belongs to the B-part of a C at offset 8."
pTypeDescriptor — the actual type_info object; spare is unused, name is the mangled name (.?AVCat@@ = class Cat, .?AU = struct, .?AW4 = enum). typeid(...).name() returns it raw; UnDecorateSymbolName prettifies it.
BaseClassDescriptor (BCD) — one per ancestor in the hierarchy. PMD (Pointer-to-Member Dispersion) holds the three displacements needed to compute the correct this pointer for that base: member offset, vbtable displacement (-1 if no virtual base), displacement inside the vbtable. numContainedBases handles nested non-polymorphic bases that share one layout.
attributes (BCD) — access/inheritance flags (public is 0; private/protected have bits set).
It's called Complete Object Locator because it locates the most-derived object: from any subobject's vftable, the COL identifies the full class and where this subobject sits inside it.

3) x64 differences — the important one for RE

struct RTTICompleteObjectLocator {   // x64 version
    DWORD signature;          // 0x00000001
    DWORD offset;
    DWORD cdOffset;
    DWORD pTypeDescriptor;    // RVA now, not VA!
    DWORD pClassDescriptor;   // RVA
    DWORD pSelf;              // x64 only: RVA of the COL itself
};

x64 stores image-relative RVAs instead of absolute VAs (and BCD/CHD/TD pointers likewise). Why: no relocation fixups needed for RTTI data → ASLR-friendly. The bonus trick: at runtime you can recover the image base from any COL:

imageBase = address_of_COL_in_memory − COL.pSelf

then translate every other RVA. This is why RTTI still resolves correctly in dumped/rebased binaries.

4) How dynamic_cast actually consumes it

Reads vfptr from the object → goes to slot [-1] → gets the COL.
Takes pClassDescriptor → walks the BaseClassArray, comparing each BCD's TypeDescriptor against the target type's descriptor (pointer compare — descriptors are unique per type).
If found: computes the adjusted this using the PMD displacements (+ offset for cross-casts) → returns the properly shifted pointer. If not found: returns nullptr (and throws for references).
typeid is the same flow minus steps 2–3: straight to pTypeDescriptor.

