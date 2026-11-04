function(vtablespp_add_example target)
    add_executable(${target} ${ARGN})

    target_link_libraries(
        ${target}
        PRIVATE vtablespp::vtablespp
    )

    option(VTABLESPP_ENABLE_SANITIZERS "Enable ASan and UBSan for examples" OFF)

    if(VTABLESPP_ENABLE_SANITIZERS)
        if(MSVC) # ASan only
            target_compile_options(${target} PRIVATE /fsanitize=address)
            target_link_options(${target} PRIVATE /fsanitize=address)
            target_compile_options(${target} PRIVATE /Zi) # Debug info
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang") # ASan + UBSan
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()

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
