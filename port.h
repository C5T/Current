// Cross-platform portability header.
//
// Ensures that one and only one of BRICKS_{POSIX,APPLE,ANDROID} is defined.
// Keeps the one provided externally. Defaults to environmental setting if none has been defined.

#ifndef BRICKS_PORT_H
#define BRICKS_PORT_H

#include <string>

#ifdef BRICKS_PORT_COUNT
#error "`BRICKS_PORT_COUNT` should not be defined for port.h"
#endif

#define BRICKS_PORT_COUNT (defined(BRICKS_POSIX) + defined(BRICKS_APPLE) + defined(BRICKS_JAVA))

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
#else
#error "Unknown architecture."
#endif

#endif
