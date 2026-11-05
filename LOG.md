
[11/02/2026]
- Fix CTest infinite run handling in vs code:
https://github.com/microsoft/vscode-cmake-tools/commit/8867be5cfe26d08c44a8c7d0388cde25621125d8

- Fix default Run CTest detect only 3 tests: add include(CTest) in CMakeLists.txt

- Steps to fix some minor issues in VS Code:
    CMake: Configure
    Developer: Reload Window

- Add ASan sanitizer for project (only for examples/).
    Have to run build from x64 dev tools to work with that flag
    -DVTABLESPP_ENABLE_SANITIZERS=ON
    How to add ASan sanitizer in system:
    Visual Studio Installer -> add sanitizer

