#pragma once

#include <version>

#define VTABLESPP_VERSION_MAJOR 0
#define VTABLESPP_VERSION_MINOR 1
#define VTABLESPP_VERSION_PATCH 0

#if defined(_MSC_VER)
#  define VTABLESPP_ABI_MSVC 1
#elif defined(__clang__)
#  define VTABLESPP_ABI_CLANG 1
#elif defined(__GNUC__)
#  define VTABLESPP_ABI_GCC 1
#else
#  define VTABLESPP_ABI_UNKNOWN 1
#endif

#if defined(_WIN64)
#  define VTABLESPP_ARCH_64 1
#elif defined(_WIN32)
#  define VTABLESPP_ARCH_32 1
#endif