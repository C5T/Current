/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef CURRENT_BRICKS_UTIL_LOCK_H
#define CURRENT_BRICKS_UTIL_LOCK_H

#include "../../port.h"

#include <mutex>
#include <type_traits>

namespace current {
namespace locks {

enum class MutexLockStatus : bool { NeedToLock = false, AlreadyLocked = true };

struct NoOpLock {
  template <typename... ARGS>
  NoOpLock(ARGS&&...) {}
};

template <MutexLockStatus MLS, class MUTEX = std::mutex>
using SmartMutexLockGuard =
    std::conditional_t<MLS == MutexLockStatus::NeedToLock, std::lock_guard<MUTEX>, NoOpLock>;

static_assert(std::is_same_v<std::lock_guard<std::mutex>, SmartMutexLockGuard<MutexLockStatus::NeedToLock>>, "");
static_assert(std::is_same_v<NoOpLock, SmartMutexLockGuard<MutexLockStatus::AlreadyLocked>>, "");

}  // namespace locks
}  // namespace current

#endif  // CURRENT_BRICKS_UTIL_LOCK_H
