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
