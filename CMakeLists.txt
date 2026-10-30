cmake_minimum_required(VERSION 3.21)

project(vtablespp
    VERSION 0.1.0
    DESCRIPTION "Library for inspecting and working with C++ virtual tables"
    LANGUAGES CXX)

option(VTABLESPP_BUILD_TESTS "Build vtablespp tests" ON)
option(VTABLESPP_BUILD_EXAMPLES "Build vtablespp examples" ON)
option(VTABLESPP_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_library(vtablespp INTERFACE)
add_library(vtablespp::vtablespp ALIAS vtablespp)

target_compile_features(vtablespp INTERFACE cxx_std_20)

target_include_directories(vtablespp
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

if(MSVC)
    target_compile_definitions(vtablespp INTERFACE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS)
endif()

if(VTABLESPP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(VTABLESPP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(DIRECTORY include/ DESTINATION include)