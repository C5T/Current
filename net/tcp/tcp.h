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

#ifndef BRICKS_NET_TCP_TCP_H
#define BRICKS_NET_TCP_TCP_H

#include "../../port.h"

#ifndef BRICKS_DEBUG_NET

#define BRICKS_DEBUG_LOG(...)

#else

// A somewhat dirty yet safe implementation of a thread-safe logger that can be enabled via a #define.
#include <cstdio>
#include <iostream>
#include <thread>
#include <mutex>
#include "../../util/singleton.h"
struct DebugLogMutex {
  std::mutex mutex;
};
#define BRICKS_DEBUG_LOG(...)                                                      \
  ([=] {                                                                           \
    std::unique_lock<std::mutex> lock(::bricks::Singleton<DebugLogMutex>().mutex); \
    std::cout << 'T' << std::this_thread::get_id() << ' ';                         \
    printf(__VA_ARGS__);                                                           \
    fflush(stdout);                                                                \
  }())

#endif

#if defined(BRICKS_POSIX) || defined(BRICKS_APPLE) || defined(BRICKS_JAVA) || defined(BRICKS_WINDOWS)
#include "impl/posix.h"
#elif defined(BRICKS_ANDROID)
#error "tcp.h should not be included in ANDROID builds."
#else
#error "No implementation for `net/tcp.h` is available for your system."
#endif

#endif  // BRICKS_NET_TCP_TCP_H
