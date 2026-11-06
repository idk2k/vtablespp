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

vtablespp_add_example(basic_example basic_example.cpp)
vtablespp_add_example(multiple_inheritance multiple_inheritance.cpp)
vtablespp_add_example(thunks_generation thunks_generation.cpp)

vtablespp_add_example(return_adjustment_thunk return_adjustment_thunk.cpp)
vtablespp_add_example(v_inheritance_thunk v_inheritance_thunk.cpp)
