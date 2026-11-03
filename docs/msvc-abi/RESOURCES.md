# Resources

Sources and references used in the `vtablespp` ABI research documents.

## Microsoft Learn — official documentation

- [The this pointer](https://learn.microsoft.com/en-us/cpp/cpp/this-pointer) —
  the C++-level definition of `this`: its type inside a member function
  and how it relates to the implicit object parameter.

- [Multiple Base Classes](https://learn.microsoft.com/en-us/cpp/cpp/multiple-base-classes) —
  multiple inheritance, base subobjects, and virtual base classes.

- [Virtual Base Classes](https://learn.microsoft.com/en-us/cpp/cpp/virtual-base-classes) —
  how virtual inheritance shares a single base subobject.

- [x64 calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention) —
  register usage on x64 Windows: the first integer or pointer argument
  (including `this`) is passed in `rcx`; the return value is in `rax`.

- [View disassembly code in the Visual Studio debugger](https://learn.microsoft.com/en-us/visualstudio/debugger/how-to-use-the-disassembly-window) —
  the Disassembly window, symbol names, and address-level debugging.

- [Symbol / PDB files in the Visual Studio debugger](https://learn.microsoft.com/en-us/visualstudio/debugger/specify-symbol-dot-pdb-and-source-files-in-the-visual-studio-debugger) —
  how PDB files map addresses to function and type names.

- [Navigate code with the debugger](https://learn.microsoft.com/en-us/visualstudio/debugger/navigating-through-code-with-the-debugger) —
  Modules window, breakpoints, and debugger navigation.

## C++ language references

- [cppreference: The this pointer](https://en.cppreference.com/w/cpp/language/this) —
  `this` as the address of the implicit object parameter; its type is
  `cv T*` where `T` is the class declaring the member function.

- [cppreference: Implicit conversions](https://en.cppreference.com/w/cpp/language/implicit_cast) —
  implicit pointer conversions between base and derived classes.

- [C++ working draft: Virtual functions](https://eel.is/c++draft/class.virtual) —
  the standard rules for overriding, covariant return types, and the
  final overrider.

- [C++ working draft: Derived classes](https://eel.is/c++draft/class.derived) —
  inheritance, base subobjects, and pointer conversions.

## ABI and thunks

- [Raymond Chen — Adjustor thunks (The Old New Thing)](https://devblogs.microsoft.com/oldnewthing/20040206-00/?p=40723/) —
  the classic explanation of adjustor thunks: how one function can be
  reachable through multiple vftables with different `this` forms.

- [Raymond Chen — Pointers to member functions are very strange animals (The Old New Thing)](https://devblogs.microsoft.com/oldnewthing/20040209-00/?p=40713/) —
  how `this` forms differ between base subobjects in multiple inheritance.

- [Itanium C++ ABI](https://itanium-cxx-abi.github.io/cxx-abi/) —
  the ABI used by GCC/Clang on Linux. Included for contrast: its vtable
  layout and thunk model differ from MSVC.

- [MSVC RTTI internals (OpenRCE)](https://www.openrce.org/articles/full_view/23) —
  the structure of MSVC RTTI: `TypeDescriptor`,
  `BaseClassDescriptor`, `ClassHierarchyDescriptor`, and
  `CompleteObjectLocator`; their placement relative to vftables.

## Project documents

- `docs/msvc-abi.md` — vftable layout, multiple inheritance, covariant
  return thunks, and verified debugger experiments.

- `docs/this-pointer-msvc-abi.md` — the two levels of `this`:
  the C++ type inside the function body versus the ABI form passed in
  `rcx`.

- `docs/why-thunk-needed.md` — why return adjustment must be bound to the
  vftable entry rather than performed by the caller.

- `docs/vftable-slot-selection-and-thunk-logic.md` — how the compiler
  selects between a direct entry and a thunk based on the static caller
  contract.

## Project repository

- [vtablespp](https://github.com/idk2k/vtablespp) —
  the library for exploring vftable internals on MSVC x64.
