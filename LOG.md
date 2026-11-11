# vtablespp development log

Date: 2026-11-05
Author: Dmitriy S. <@idk2k>


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

[11/05/2026]
- Split tests into separate files:
    tests/vtable_view_test.cpp
    tests/slot_classifier_test.cpp
    tests/msvc_vtable_test.cpp

- Add slot classification API:
    slot_kind, slot_info, slot_classifier::inspect()

- Implement conservative x64 this-adjustor detection:
    match `sub rcx, imm8/imm32` + `jmp rel32` / `jmp [rip+rel32]`

- Add safe executable-memory reader:
    abi::read_code() using VirtualQuery + SEH

- Add pure virtual call detection:
    abi::purecall_address() and slot_kind::pure_call

- Add incremental-linking jump stub (/INCREMENTAL) resolution:
    resolve one `E9 rel32` jump before thunk classification

- Add vtable_view::from_base<Base>() to access secondary base vftables
  without hardcoding base-subobject offsets

- Change vtable_view::at() to return std::optional:
    distinguishes “slot does not exist” from “slot contains nullptr”

- Add GoogleTest coverage:
    vtable_view API
    synthetic x64 thunk patterns (bytes sequence)
    real MSVC multiple-inheritance cases
    shared override -> this_adjustor{8}
    separate base implementations -> no thunk
    pure virtual slot -> pure_call

- How to fix VS Code IntelliSense phantom errors:
    CMake: Delete Cache and Reconfigure
    C/C++: Reset IntelliSense Database

==================================
- Next steps (TODO):
    return-adjustor detection
    direct-entry classification
    call-by-slot
    vftable size detection
    RTTI parsing
    virtual inheritance (with thunk support - it uses vbtable)
    hooking (last)
==================================

[11/06/2026]

- add return-adjustor detection (RAD)
- add tests for RAD
- add better documentation for this adjustor thunk (TAT) and (RAT)

==================================
- Next steps (TODO):
    RTTI parsing
    direct-entry classification
    call-by-slot
    vftable size detection
    virtual inheritance (with thunk support - it uses vbtable)
    hooking (last)
==================================

[11/09/2026]

==================================
- Next steps (TODO):
    direct-entry classification
    call-by-slot
    RTTI parsing   
    vftable size detection
    virtual inheritance (with thunk support - it uses vbtable)
    hooking (last)
==================================

[11/10/2026]

- added direct-entry classification (maybe poor, need MORE TESTS)
- added call-by-slot (need MORE EXAMPLES)

==================================
- Next steps (TODO):  
    RTTI parsing   
    vftable size detection
    hooking (last)
    virtual inheritance (with thunk support - it uses vbtable)
==================================

[11/11/2026]


TODO:
1. symbol_resolver: адрес → symbol name, displacement, module.
2. Extend slot_info: symbol_name, symbol_displacement, module.
3. Demangling using UnDecorateSymbolName.