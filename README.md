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

## Important caveat

Virtual table layout, RTTI layout, thunk generation, and multiple/virtual inheritance
behavior are implementation-defined. Do not use this library to mutate vtables of
objects whose lifetime and ABI layout you do not fully control.

## License

AGPL-3.0. See `LICENSE.txt`.
