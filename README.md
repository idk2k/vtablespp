# vtablespp

**vtablespp** is an experimental C++20 library for inspecting compiler-generated
virtual method tables and RTTI metadata.

The initial focus is the Microsoft Visual C++ ABI on Windows, especially x64.
The project is intended for reverse engineering, diagnostics, educational tooling,
and ABI research—not for relying on non-portable object layouts in production code.

## Features

- C++20 API
- Header-first core library
- CMake-based build
- MSVC-focused initial implementation
- GoogleTest-based test suite
- Examples for single and multiple inheritance

## Build on Windows / MSVC

Requirements:

- CMake 3.21+
- Visual Studio 2022 with C++ workload
- Git

```bat
build.bat
```

Release build:

```bat
build.bat release
```

Clean rebuild:

```bat
build.bat release clean
```

Manual CMake workflow:

```bat
cmake --preset msvc-x64-debug
cmake --build --preset msvc-x64-debug --config Debug
ctest --preset msvc-x64-debug
```

### 🛡️ Building with AddressSanitizer (ASan)

To enable **AddressSanitizer** for the examples, use the `VTABLESPP_ENABLE_SANITIZERS` option. This is useful for detecting memory issues while exploring vftable internals.

> **Note:** ASan on MSVC requires disabling incremental linking (`/INCREMENTAL:NO`) to ensure stable function addresses and avoid extra jump stubs that might interfere with thunk detection logic.

#### Using `build.bat`
Pass the CMake definition directly as an argument:

```bash
# Debug build with ASan enabled
build.bat -DVTABLESPP_ENABLE_SANITIZERS=ON

# Clean rebuild with ASan
build.bat clean -DVTABLESPP_ENABLE_SANITIZERS=ON
```

If you configure manually, ensure you are in a Debug configuration:
```cmake
cmake --preset msvc-x64-debug -DVTABLESPP_ENABLE_SANITIZERS=ON
cmake --build --preset msvc-x64-debug --config Debug
```
Requirements:
- The build script automatically copies clang_rt.asan_dynamic-x86_64.dll to the output directory.
- Make sure your environment has access to the Visual Studio runtime libraries.


## Example

```cpp
#include <vtablespp/vtablespp.hpp>

struct Base {
    virtual ~Base() = default;
    virtual int test() const { return 1; }
};

struct Derived final : Base {
    int test() const override { return 2; }
};

int main() {
    Derived object;
    const void* vtable = vtablespp::vtable_of(object);
    return vtable != nullptr ? 0 : 1;
}
```

## Interactive example

`basic_example` is a small interactive console application for exploring
object addresses, base subobjects, and virtual function tables with the
current MSVC ABI backend.

### Single inheritance

This experiment creates two separate objects:

- a `Base` object;
- a `Derived` object, inspected through a `const Base&`.

For each object, the application prints:

- the address of the inspected object or base subobject;
- the address of its virtual function table;
- the number of inspected slots;
- the address stored in each inspected slot.

It also performs ordinary virtual calls so that their results can be
compared with the displayed table information.

Viewing a `Derived` object through a `Base` reference does not create a
new object or refer to the separate `Base` instance. The reference binds
to the `Base` subobject within the existing `Derived` object.

### Multiple inheritance

This experiment creates one `MultiDerived` object and inspects it through
references to its `Left` and `Right` base subobjects.

Compare:

- the complete object address;
- the addresses of the two base subobjects;
- the virtual function table addresses obtained from those subobjects.

The example obtains base references using normal C++ conversions rather
than assuming fixed byte offsets.

### Inspecting memory in a debugger

To compare the output with raw memory, set a breakpoint inside
`show_single_inheritance()` or `show_multiple_inheritance()`, after the
table information has been printed and before the function returns.

Use addresses from the current execution. The sample objects are local
variables, so their printed addresses must not be treated as valid object
addresses after the function has returned.

On the targeted MSVC x64 configuration, inspect eight bytes when reading
a table pointer. In a byte-oriented memory view, pointer values appear
in little-endian order.

### Scope and limitations

- This is an ABI-specific experiment, not a portable C++ reflection API.
- Each table view inspects one explicitly supplied slot.
- The example does not discover or validate the actual table length.
- The fixtures intentionally omit virtual destructors and are not deleted
  through base pointers.
- Extracted entry addresses are displayed, not invoked.
- Ordinary virtual calls are made through the declared C++ interfaces.
- Exact addresses and layout details may differ between builds and runs.

## Important caveat

Virtual table layout, RTTI layout, thunk generation, and multiple/virtual inheritance
behavior are implementation-defined. Do not use this library to mutate vtables of
objects whose lifetime and ABI layout you do not fully control.

## License

AGPL-3.0. See `LICENSE.txt`.
