add_executable(basic_example basic_example.cpp)
target_link_libraries(basic_example PRIVATE vtablespp::vtablespp)

if(MSVC)
    target_compile_options(basic_example PRIVATE /W4 /permissive-)
endif()