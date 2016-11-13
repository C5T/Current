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
// Ensures that one and only one of CURRENT_{POSIX,APPLE,ANDROID} is defined.
// Keeps the one provided externally. Defaults to environmental setting if none has been defined.

#ifndef CURRENT_PORT_H
#define CURRENT_PORT_H

// TODO(dkorolev): Needed by Nathan. Remove later.
#include <cassert>

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

#ifdef CURRENT_PORT_COUNT
#error "`CURRENT_PORT_COUNT` should not be defined for port.h"
#endif

#define CURRENT_PORT_COUNT \
  (defined(CURRENT_POSIX) + defined(CURRENT_APPLE) + defined(CURRENT_JAVA) + defined(CURRENT_WINDOWS))

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

// `std::make_unique` exists in C++14, but for Current we'd like to see it "supported" in C++11. -- D.K.
namespace add_some_of_cpp14_into_cpp11 {
template <typename T, typename... PARAMS>
std::unique_ptr<T> make_unique(PARAMS&&... params) {
  return std::unique_ptr<T>(new T(std::forward<PARAMS>(params)...));
}
template <bool B, class T = void>
using enable_if_t = typename std::enable_if<B, T>::type;
}  // namespace add_some_of_cpp14_into_cpp11

namespace std {
using namespace add_some_of_cpp14_into_cpp11;
}  // namespace std

// The best way I found to have clang++ dump the actual type in error message. -- D.K.
// Usage: static_assert(sizeof(is_same_or_compile_error<A, B>), "");
template <typename T1, typename T2>
struct is_same_or_compile_error {
  enum { value = std::is_same<T1, T2>::value };
  char is_same_static_assert_failed[value ? 1 : -1];
};

// Unit test ports begin with 19999 by default and go down from there.
#define PickPortForUnitTest() PortForUnitTestPicker::PickOne()
struct PortForUnitTestPicker {
  static int PickOne() {
    static int port = 20000;
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
}

#ifndef CURRENT_ASSERT
#define CURRENT_ASSERT(expr) ((expr) ? static_cast<void>(0) : CURRENT_ASSERTION_FAILED(#expr, __FILE__, __LINE__))
#endif

// The `FEWER_COMPILE_TIME_CHECKS` macro, when set, moves more checks from compile time to run time.
// Pro: Faster and less RAM-demanding compilation. Con: Some type errors may go unnoticed.
// TODO(dkorolev)+TODO(mzhurovich): We'd like to bind this to `!NDEBUG`, but let's keep it on by default for now.
#define FEWER_COMPILE_TIME_CHECKS

#ifdef CURRENT_PORT_H_CRT_SECURE_NO_WARNINGS_FLAG_REQUIRES_UNSET
#undef _CRT_SECURE_NO_WARNINGS
#undef CURRENT_PORT_H_CRT_SECURE_NO_WARNINGS_FLAG_REQUIRES_UNSET
#endif  // CURRENT_PORT_H_CRT_SECURE_NO_WARNINGS_FLAG_REQUIRES_UNSET

#endif
