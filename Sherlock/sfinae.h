/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef SHERLOCK_SFINAE_H
#define SHERLOCK_SFINAE_H

#include <cassert>

#include "../Bricks/cerealize/cerealize.h"

namespace sherlock {
namespace sfinae {

// TODO(dkorolev): Move this to Bricks. Cerealize uses it too, for `WithBaseType`.
template <typename T>
struct PretendingToBeUniquePtr {
  struct NullDeleter {
    void operator()(T&) {}
    void operator()(T*) {}
  };
  std::unique_ptr<T, NullDeleter> non_owned_instance_;
  PretendingToBeUniquePtr() = delete;
  PretendingToBeUniquePtr(T& instance) : non_owned_instance_(&instance) {}
  PretendingToBeUniquePtr(PretendingToBeUniquePtr&& rhs)
      : non_owned_instance_(std::move(rhs.non_owned_instance_)) {
    assert(non_owned_instance_);
    assert(!rhs.non_owned_instance_);
  }
  T* operator->() {
    assert(non_owned_instance_);
    return non_owned_instance_.operator->();
  }
  T& operator*() {
    assert(non_owned_instance_);
    return non_owned_instance_.operator*();
  }
};

}  // namespace sherlock::sfinae
}  // namespace sherlock

#endif  // SHERLOCK_SFINAE_H
