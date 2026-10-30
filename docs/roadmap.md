# Roadmap

## v0.1 — Foundation

- CMake build
- MSVC x64 build script
- Basic vtable pointer extraction
- Basic examples
- GoogleTest integration

## v0.2 — MSVC RTTI

- Parse `RTTICompleteObjectLocator`
- Parse `RTTITypeDescriptor`
- Extract demangled-ish class names
- Detect RTTI-enabled builds

## v0.3 — Hierarchy model

- Model primary and secondary vtables
- Detect base subobject offsets
- Support multiple inheritance fixtures
- Document MSVC-specific caveats

## v0.4 — Advanced ABI

- Virtual inheritance and vbtables
- x86 support
- Compiler-version compatibility matrix

## v0.5 — Portability

- GCC / Itanium C++ ABI
- Clang / Itanium C++ ABI
- Platform-independent public API
