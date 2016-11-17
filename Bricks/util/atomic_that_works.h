/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

// http://stackoverflow.com/questions/23065501/stdatomicunsiged-long-long-undefined-reference-to-atomic-fetch-add-8

#ifndef BRICKS_UTIL_ATOMIC_THAT_WORKS_H
#define BRICKS_UTIL_ATOMIC_THAT_WORKS_H

#include <cstring>
#include <mutex>

namespace current {

template <typename T>
class atomic_that_works {
 public:
  atomic_that_works() = default;
  atomic_that_works(const T& value) : value_(value) {}
  atomic_that_works(const atomic_that_works&) = delete;
  atomic_that_works(atomic_that_works&&) = delete;

  T load() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return value_;
  }

  T& load_into(T& destination) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::memcpy(&destination, &value_, sizeof(T));
    return destination;
  }

  void store(const T& new_value) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::memcpy(&value_, &new_value, sizeof(T));
  }

 private:
  T value_;
  mutable std::mutex mutex_;
};

}  // namespace current

#endif  // BRICKS_UTIL_ATOMIC_THAT_WORKS_H
