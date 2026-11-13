# ------------------- Sanitizers -------------------
option(VTABLESPP_ENABLE_SANITIZERS "Enable ASan and UBSan for examples" OFF)

if (MSVC AND VTABLESPP_ENABLE_SANITIZERS)
    add_compile_options(/fsanitize=address)
    add_link_options("$<$<CONFIG:Debug>:/INCREMENTAL:NO>")
endif()

if (MSVC AND VTABLESPP_ENABLE_SANITIZERS)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "ProgramDatabase")
    get_filename_component(MSVC_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)

    # IF YOU HAVE NO x64 DevTools from VS then uncomment it:
    # if (EXISTS "${MSVC_BIN_DIR}/clang_rt.asan_dynamic-x86_64.dll")
    #     file(COPY "${MSVC_BIN_DIR}/clang_rt.asan_dynamic-x86_64.dll" 
    #         DESTINATION "${CMAKE_BINARY_DIR}/examples/Debug")
    # endif()
endif()
# ------------------- Sanitizers -------------------

function(vtablespp_add_example target)
    add_executable(${target} ${ARGN})

    target_link_libraries(
        ${target}
        PRIVATE vtablespp::vtablespp
    )

    if(MSVC)
        target_compile_options(
            ${target}
            PRIVATE /W4 /permissive-
        )
    else()
        target_compile_options(
            ${target}
            PRIVATE -Wall -Wextra -Wpedantic
        )
    endif()
endfunction()

# Basics
vtablespp_add_example(basic_example basic_example.cpp)
vtablespp_add_example(multiple_inheritance multiple_inheritance.cpp)

# Thunks in normal situation
vtablespp_add_example(this_adjustment_thunk this_adjustment_thunk.cpp)
vtablespp_add_example(return_adjustment_thunk return_adjustment_thunk.cpp)

# Thunks in virtual inheritance scnenario
vtablespp_add_example(virtual_inheritance_ra_thunk virtual_inheritance_ra_thunk.cpp)

# Other
vtablespp_add_example(testing testing.cpp)

vtablespp_add_example(symbol_resolver_demo symbol_resolver_demo.cpp)

target_compile_options(symbol_resolver_demo PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/Zi>
)

target_link_options(symbol_resolver_demo PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/DEBUG>
)

# ---
vtablespp_add_example(vtable_size_detection vtable_size_detection.cpp)