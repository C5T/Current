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

// TODO(dkorolev): @deathbaba mentioned this `#define` helps with some issues on Mac,
// I have not enconutered those yet. Uncomment once we confirm them. -- D.K.
// #define __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES 0

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

// `std::make_unique` exists in C++14, but for Bricks we'd like to see it "supported" in C++11. -- D.K.
namespace make_unique_for_poor_cpp11_users_impl {
template <typename T>
std::unique_ptr<T> make_unique(T* ptr) {
  return std::unique_ptr<T>(ptr);
}
}  // namespace make_unique_for_poor_cpp11_users_impl
using namespace make_unique_for_poor_cpp11_users_impl;

#endif
