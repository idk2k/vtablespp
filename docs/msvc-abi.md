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
