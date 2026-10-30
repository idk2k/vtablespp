include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(vtablespp_tests
    vtable_test.cpp
)

target_link_libraries(vtablespp_tests
    PRIVATE
        vtablespp::vtablespp
        GTest::gtest_main
)

if(MSVC)
    target_compile_options(vtablespp_tests PRIVATE /W4 /permissive-)
endif()

include(GoogleTest)
gtest_discover_tests(vtablespp_tests)