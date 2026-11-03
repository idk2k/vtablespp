function(vtablespp_add_example target source)
    add_executable(${target} ${source})

    target_link_libraries(
        ${target}
        PRIVATE vtablespp::vtablespp
    )

    if(MSVC)
        target_compile_options(
            ${target}
            PRIVATE /W4 /permissive-
        )
    endif()
endfunction()

vtablespp_add_example(basic_example basic_example.cpp)
vtablespp_add_example(multiple_inheritance multiple_inheritance.cpp)
vtablespp_add_example(call_multiple_inheritance call_multiple_inheritance.cpp)