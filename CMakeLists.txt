cmake_minimum_required(VERSION 3.21)

project(vtablespp
    VERSION 0.1.0
    DESCRIPTION "Library for working with vtables internals"
    LANGUAGES CXX)

include(CTest)

option(VTABLESPP_BUILD_TESTS "Build vtablespp tests" ON)
option(VTABLESPP_BUILD_EXAMPLES "Build vtablespp examples" ON)
option(VTABLESPP_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_library(vtablespp STATIC)
add_library(vtablespp::vtablespp ALIAS vtablespp)

target_compile_features(vtablespp PUBLIC cxx_std_20)

target_include_directories(vtablespp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_sources(vtablespp PRIVATE
    src/abi/msvc/abi_windows.cpp
    src/core/slot_classifier.cpp
    src/symbols/msvc/symbol_resolver_windows.cpp)

if(MSVC)
    target_compile_definitions(vtablespp PUBLIC
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(vtablespp PUBLIC user32)
    target_link_libraries(vtablespp PUBLIC kernel32)
    target_link_libraries(vtablespp PRIVATE Dbghelp.lib)
endif()

if(VTABLESPP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(VTABLESPP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(DIRECTORY include/ DESTINATION include)