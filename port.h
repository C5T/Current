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

#ifdef CURRENT_WINDOWS
// NOTE(dkorolev): Added in March 2021 while making sure Visual Studio compiles Current fine.
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#endif

// #define CURRENT_STORAGE_PATCH_SUPPORT

// Cross-platform portability header.
//
// Ensures that one and only one of CURRENT_{POSIX,APPLE,ANDROID} is defined.
// Keeps the one provided externally. Defaults to environmental setting if none has been defined.

#ifndef CURRENT_PORT_H
#define CURRENT_PORT_H

#define NOMINMAX  // Tell Visual Studio to not mess with std::min() / std::max().

#ifdef _MSC_VER
// clang-format off
#pragma warning(disable:4503)  // "decorated name length exceeded ...", duh.
// clang-format on
#ifndef _CRT_SECURE_NO_WARNINGS
#define CURRENT_PORT_H_CRT_SECURE_NO_WARNINGS_FLAG_REQUIRES_UNSET
#define _CRT_SECURE_NO_WARNINGS
#endif  // !_CRT_SECURE_NO_WARNINGS
#endif

// TODO(dkorolev): @deathbaba mentioned this `#define` helps with some issues on Mac,
// I have not enconutered those yet. Uncomment once we confirm them. -- D.K.
// #define __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES 0

#include <limits>  // For data type implementation details.
#include <string>  // For architecture names.
#include <memory>  // For `std::unique_ptr`.
#include <type_traits> // For `std::is_same_v`

#ifdef CURRENT_PORT_COUNT
#error "`CURRENT_PORT_COUNT` should not be defined for port.h"
#endif

#ifdef CURRENT_POSIX
#define COUNT_CURRENT_POSIX_DEFINED 1
#else
#define COUNT_CURRENT_POSIX_DEFINED 0
#endif

#ifdef CURRENT_APPLE
#define COUNT_CURRENT_APPLE_DEFINED 1
#else
#define COUNT_CURRENT_APPLE_DEFINED 0
#endif

#ifdef CURRENT_JAVA
#define COUNT_CURRENT_JAVA_DEFINED 1
#else
#define COUNT_CURRENT_JAVA_DEFINED 0
#endif

#ifdef CURRENT_WINDOWS
#define COUNT_CURRENT_WINDOWS_DEFINED 1
#else
#define COUNT_CURRENT_WINDOWS_DEFINED 0
#endif

#define CURRENT_PORT_COUNT \
   (COUNT_CURRENT_POSIX_DEFINED + COUNT_CURRENT_APPLE_DEFINED + COUNT_CURRENT_JAVA_DEFINED + COUNT_CURRENT_WINDOWS_DEFINED)

#if defined(ANDROID)

#if CURRENT_PORT_COUNT != 0
#error "Should not define any `CURRENT_*` macros when buliding for Android."
#else
#define CURRENT_ANDROID
#endif

#else  // !defined(ANDROID)

#if CURRENT_PORT_COUNT > 1
#error "More than one `CURRENT_*` architectures have been defined."
#elif CURRENT_PORT_COUNT == 0

#if defined(__linux)
#define CURRENT_POSIX
#elif defined(__APPLE__)
#define CURRENT_APPLE
#elif defined(_WIN32)
#define CURRENT_WINDOWS
#else
#error "Could not detect architecture. Please define one of the `CURRENT_*` macros explicitly."
#endif

#endif  // `CURRENT_PORT_COUNT == 0`

#endif  // #elif of `defined(ANDROID)`

#undef CURRENT_PORT_COUNT
#undef COUNT_CURRENT_POSIX_DEFINED
#undef COUNT_CURRENT_APPLE_DEFINED
#undef COUNT_CURRENT_JAVA_DEFINED
#undef COUNT_CURRENT_WINDOWS_DEFINED

#if defined(CURRENT_POSIX)
#define CURRENT_ARCH_UNAME std::string("Linux")
#define CURRENT_ARCH_UNAME_AS_IDENTIFIER Linux
#elif defined(CURRENT_APPLE)
#define CURRENT_ARCH_UNAME std::string("Darwin")
#define CURRENT_ARCH_UNAME_AS_IDENTIFIER Darwin
#elif defined(CURRENT_JAVA)
#define CURRENT_ARCH_UNAME std::string("Java")
#define CURRENT_ARCH_UNAME_AS_IDENTIFIER Java
#elif defined(CURRENT_ANDROID)
#define CURRENT_ARCH_UNAME std::string("Android")
#define CURRENT_ARCH_UNAME_AS_IDENTIFIER Android
#elif defined(CURRENT_WINDOWS)
#define CURRENT_ARCH_UNAME std::string("Windows")
#define CURRENT_ARCH_UNAME_AS_IDENTIFIER Windows
#else
#error "Unknown architecture."
#endif

#ifdef CURRENT_WINDOWS

// #include <windows.h>  -- `#include <windows.h>` does not play well with WSA -- D.K.

// These two headers seem to work fine for both WSA and `FindFirst*` files.
#include <WS2tcpip.h>
#include <corecrt_io.h>

#undef DELETE  // Yep. <winnt.h> does `#define DELETE (0x00010000L)`. I know right?

// Visual Studio does not define `M_PI`.
#ifndef M_PI
#define M_PI 3.14159265359
#endif

#endif  // CURRENT_WINDOWS

#ifdef CURRENT_APPLE

// The following line is needed to avoid OS X headers conflicts with C++.
#define __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES 0

#ifdef CURRENT_APPLE_ENABLE_NSLOG
#define CURRENT_NSLOG(...) ::NSLog(__VA_ARGS__)
#else
inline void CURRENT_NSLOG(...) {}
#endif

#endif  // CURRENT_APPLE

// Check for 'thread_local' specifier support.
#ifdef __clang__
#if __has_feature(cxx_thread_local)
#define CURRENT_HAS_THREAD_LOCAL
#endif
#elif !defined(CURRENT_ANDROID)
#define CURRENT_HAS_THREAD_LOCAL
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

// The best way I found to have clang++ dump the actual type in error message. -- D.K.
// Usage: static_assert(sizeof(is_same_or_compile_error<A, B>), "");
template <typename T1, typename T2>
struct is_same_or_compile_error {
  enum { value = std::is_same_v<T1, T2> };
  char is_same_static_assert_failed[value ? 1 : -1];
};

// Unit test ports begin with 19999 by default and go down from there.
#define PickPortForUnitTest() PortForUnitTestPicker::PickOne()
struct PortForUnitTestPicker {
  static uint16_t PickOne() {
    static uint16_t port = 20000;
    return --port;
  }
};

#ifdef CURRENT_JAVA
#error "Current has not been tested with Java for a while, and would likely require a bit of TLC. Thank you."
#endif

// We added `ChunkedGET` alongside `GET` into our `HTTP` library. Sadly, it only supports the POSIX,
// not Mac native, implementation. Thus, to keep Current building on Mac, until we have a workaround or stub,
// force Mac builds to use POSIX HTTP client implementation.
#define CURRENT_APPLE_HTTP_CLIENT_POSIX

// Make sure `__DATE__` and `__TIME__` parsing works on both Windows and Ubuntu.
// TL;DR: Ubuntu doesn't have `std::get_time()` until gcc 5, while MSVS doesn't have strptime.
// Below is the workaround from http://stackoverflow.com/questions/321849/strptime-equivalent-on-windows.
#ifdef CURRENT_WINDOWS
#include <iomanip>
#include <sstream>
namespace enable_strptime_on_windows {
inline const char* strptime(const char* input_string, const char* input_format, tm* output_tm) {
  std::istringstream input(input_string);
  input.imbue(std::locale(setlocale(LC_ALL, nullptr)));
  input >> std::get_time(output_tm, input_format);
  if (input.fail()) {
    return nullptr;
  } else {
    return input_string + static_cast<std::ptrdiff_t>(input.tellg());
  }
}
}  // namespace enable_strptime_on_windows
using namespace enable_strptime_on_windows;
#endif  // CURRENT_WINDOWS

inline void CURRENT_ASSERTION_FAILED(const char* text, const char* file, int line) {
  fprintf(stderr, "Current assertion failed:\n\t%s\n\t%s : %d\n", text, file, line);
#ifdef CURRENT_BUILD_WITH_PARANOIC_RUNTIME_CHECKS
  volatile int* you_shalt_segfault = nullptr;
  *you_shalt_segfault = 42;
#endif  // CURRENT_BUILD_WITH_PARANOIC_RUNTIME_CHECKS
}

#ifndef CURRENT_ASSERT
#define CURRENT_ASSERT(expr) ((expr) ? static_cast<void>(0) : CURRENT_ASSERTION_FAILED(#expr, __FILE__, __LINE__))
#endif

// The `VARIANT_CHECKS_AT_RUNTIME_INSTEAD_OF_COMPILE_TIME` macro, when set, moves `Variant` type checks
// from compile time to run time.
// Pros:
// * Human-readable types in error messages, helpful for debugging.
// * Faster and less RAM-demanding compilation [ Uncertain. -- D.K. ]
// Cons:
// * Nested `Variant`-s are not supported when type checks are moved from compile time to runtime.
// * Some type errors may go unnoticed.
// #define VARIANT_CHECKS_AT_RUNTIME_INSTEAD_OF_COMPILE_TIME

#ifdef CURRENT_PORT_H_CRT_SECURE_NO_WARNINGS_FLAG_REQUIRES_UNSET
#undef _CRT_SECURE_NO_WARNINGS
#undef CURRENT_PORT_H_CRT_SECURE_NO_WARNINGS_FLAG_REQUIRES_UNSET
#endif  // CURRENT_PORT_H_CRT_SECURE_NO_WARNINGS_FLAG_REQUIRES_UNSET

// NOTE(dkorolev): Added in March 2021 while making sure Visual Studio compiles Current fine.
#ifdef CURRENT_WINDOWS
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#pragma warning(disable : 4996)  // Because the above `#define`-s don't do what they should on my Visual Studio. -- D.K.
#endif

#endif
