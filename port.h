/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

// Cross-platform portability header.
//
// Ensures that one and only one of BRICKS_{POSIX,APPLE,ANDROID} is defined.
// Keeps the one provided externally. Defaults to environmental setting if none has been defined.

#ifndef BRICKS_PORT_H
#define BRICKS_PORT_H

#define NOMINMAX  // Tell Visual Studio to not mess with std::min() / std::max().

#ifdef _MSC_VER
#pragma warning (disable:4503)  // "decorated name length exceeded ...", duh.
#define _CRT_SECURE_NO_WARNINGS
#endif

// TODO(dkorolev): @deathbaba mentioned this `#define` helps with some issues on Mac,
// I have not enconutered those yet. Uncomment once we confirm them. -- D.K.
// #define __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES 0

#include <limits>  // For data type implementation details.
#include <string>  // For architecture names.
#include <memory>  // For `std::unique_ptr` and the mimic of `make_unique`.

#ifdef BRICKS_PORT_COUNT
#error "`BRICKS_PORT_COUNT` should not be defined for port.h"
#endif

#define BRICKS_PORT_COUNT \
  (defined(BRICKS_POSIX) + defined(BRICKS_APPLE) + defined(BRICKS_JAVA) + defined(BRICKS_WINDOWS))

#if defined(ANDROID)

#if BRICKS_PORT_COUNT != 0
#error "Should not define any `BRICKS_*` macros when buliding for Android."
#else
#define BRICKS_ANDROID
#endif

#else  // !defined(ANDROID)

#if BRICKS_PORT_COUNT > 1
#error "More than one `BRICKS_*` architectures have been defined."
#elif BRICKS_PORT_COUNT == 0

#if defined(__linux)
#define BRICKS_POSIX
#elif defined(__APPLE__)
#define BRICKS_APPLE
#elif defined(_WIN32)
#define BRICKS_WINDOWS
#else
#error "Could not detect architecture. Please define one of the `BRICKS_*` macros explicitly."
#endif

#endif  // `BRICKS_PORT_COUNT == 0`

#endif  // #elif of `defined(ANDROID)`

#undef BRICKS_PORT_COUNT

#if defined(BRICKS_POSIX)
#define BRICKS_ARCH_UNAME std::string("Linux")
#define BRICKS_ARCH_UNAME_AS_IDENTIFIER Linux
#elif defined(BRICKS_APPLE)
#define BRICKS_ARCH_UNAME std::string("Darwin")
#define BRICKS_ARCH_UNAME_AS_IDENTIFIER Darwin
#elif defined(BRICKS_JAVA)
#define BRICKS_ARCH_UNAME std::string("Java")
#define BRICKS_ARCH_UNAME_AS_IDENTIFIER Java
#elif defined(BRICKS_ANDROID)
#define BRICKS_ARCH_UNAME std::string("Android")
#define BRICKS_ARCH_UNAME_AS_IDENTIFIER Android
#elif defined(BRICKS_WINDOWS)
#define BRICKS_ARCH_UNAME std::string("Windows")
#define BRICKS_ARCH_UNAME_AS_IDENTIFIER Windows
#else
#error "Unknown architecture."
#endif

#ifdef BRICKS_WINDOWS

// #include <windows.h>  -- `#include <windows.h>` does not play well with WSA -- D.K.

// These two headers seem to work fine for both WSA and `FindFirst*` files.
#include <WS2tcpip.h>
#include <corecrt_io.h>

#undef DELETE  // Yep. <winnt.h> does `#define DELETE (0x00010000L)`. I know right?

// Visual Studio does not define `M_PI`.
#ifndef M_PI
#define M_PI 3.14159265359
#endif

#endif

// Check for 'thread_local' specifier support.
#ifdef __clang__
#if __has_feature(cxx_thread_local)
#define BRICKS_HAS_THREAD_LOCAL
#endif
#elif !defined(BRICKS_ANDROID)
#define BRICKS_HAS_THREAD_LOCAL
#endif

// Current internals rely heavily on specific implementations of certain data types.
static_assert(sizeof(uint8_t) == 1u, "`uint8_t` must be exactly 1 byte.");
static_assert(sizeof(uint16_t) == 2u, "`uint16_t` must be exactly 2 bytes.");
static_assert(sizeof(uint32_t) == 4u, "`uint32_t` must be exactly 4 bytes.");
static_assert(sizeof(uint64_t) == 8u, "`uint64_t` must be exactly 8 bytes.");
static_assert(sizeof(int8_t) == 1u, "`int8_t` must be exactly 1 byte.");
static_assert(sizeof(int16_t) == 2u, "`int16_t` must be exactly 2 bytes.");
static_assert(sizeof(int32_t) == 4u, "`int32_t` must be exactly 4 bytes.");
static_assert(sizeof(int64_t) == 8u, "`int64_t` must be exactly 8 bytes.");
static_assert(std::numeric_limits<float>::is_iec559, "`float` type is not IEC-559 compliant.");
static_assert(std::numeric_limits<double>::is_iec559, "`double` type is not IEC-559 compliant.");
static_assert(sizeof(float) == 4u, "Only 32-bit `float` is supported.");
static_assert(sizeof(double) == 8u, "Only 64-bit `double` is supported.");

// `std::make_unique` exists in C++14, but for Bricks we'd like to see it "supported" in C++11. -- D.K.
namespace make_unique_for_poor_cpp11_users_impl {
template <typename T, typename... PARAMS>
std::unique_ptr<T> make_unique(PARAMS&&... params) {
  return std::unique_ptr<T>(new T(std::forward<PARAMS>(params)...));
}
}  // namespace make_unique_for_poor_cpp11_users_impl
using namespace make_unique_for_poor_cpp11_users_impl;

// The best way I found to have clang++ dump the actual type in error message. -- D.K.
// Usage: static_assert(sizeof(is_same_or_compile_error<A, B>), "");
// TODO(dkorolev): Chat with Max, remove or move it into Bricks.
template <typename T1, typename T2>
struct is_same_or_compile_error {
  enum { value = std::is_same<T1, T2>::value };
  char is_same_static_assert_failed[value ? 1 : -1];
};

#endif
