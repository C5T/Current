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

#ifndef BRICKS_OPTIONALLY_OWNED_H
#define BRICKS_OPTIONALLY_OWNED_H

#include <memory>

namespace bricks {

// Class `OptionallyOwned<T>` can be constructed from `T&` or from an `std::unique_ptr<T>`.
// It provides identical access to the underlying object, and can be move-constructed away,
// into another `OptionallyOwned<T>` object, with a common usecase being sharing an object with another thread.
//
// If `OptionallyOwned<T>` has been constructed from a `unique_ptr<T>`, the instance of `OptionallyOwned<T>`
// that this object has been passed to last owns the destruction of this particular instance of `T`.
// This makes `OptionallyOwned<T>` an ideal case for an `std::thread` what needs to `.detach()` itself
// from the caller thread, while keeping ownership of the object originally passed in.
//
// A `bool IsDetachable();` method is thus provided, which returns `true`
// for `std::unique_ptr<T>`-constructed, and `false` for `T&`-constructed instances of `OptionallyOwned<T>`.

template <typename T>
class OptionallyOwned {
 public:
  explicit OptionallyOwned(T& ref) : instance_(ref), unique_ptr_(nullptr), valid_(true) {}

  explicit OptionallyOwned(std::unique_ptr<T> ptr)
      : instance_(*ptr.get()), unique_ptr_(std::move(ptr)), valid_(true) {}

  OptionallyOwned(OptionallyOwned&& rhs)
      : instance_(rhs.instance_), unique_ptr_(std::move(rhs.unique_ptr_)), valid_(true) {
    if (!rhs.valid_) {
      // TODO(dkorolev): Custom exception type.
      throw std::logic_error("Attempting to construct from an already invalid OptionallyOwned.");
    }
    rhs.valid_ = false;
  }

  T& Ref() {
    if (!valid_) {
      throw std::logic_error("Attempted to `Ref()` an invalid OptionallyOwned.");
    }
    return instance_;
  }

  T* Ptr() {
    if (!valid_) {
      throw std::logic_error("Attempted to `Ptr()` an invalid OptionallyOwned.");
    }
    return &instance_;
  }

  operator T&() {
    if (!valid_) {
      throw std::logic_error("Attempted to `operator T&()` an invalid OptionallyOwned.");
    }

    return instance_;
  }

  operator const T&() const {
    if (!valid_) {
      throw std::logic_error("Attempted to `operator const T&()` an invalid OptionallyOwned.");
    }
    return instance_;
  }

  T* operator->() {
    if (!valid_) {
      throw std::logic_error("Attempted to `operator->()` an invalid OptionallyOwned.");
    }
    return &instance_;
  }

  bool IsValid() const { return valid_; }

  bool IsDetachable() const { return unique_ptr_.get() != nullptr; }

 private:
  // The actual instance of `T` to work with.
  T& instance_;

  // Ownership handle.
  // Non-`nullptr` if the `owned` original object has been constructed from a `unique_ptr<T>`.
  // Nullptr if the original object has been constructed from a `T&`.
  std::unique_ptr<T> unique_ptr_;

  // Extra safety check. TODO(dkorolev): Perhaps enable these checks in debug mode only?
  bool valid_;

  OptionallyOwned() = delete;
  OptionallyOwned(const OptionallyOwned&) = delete;
  void operator=(const OptionallyOwned&) = delete;
  void operator=(OptionallyOwned&&) = delete;
};

}  // namespace bricks

#endif  // BRICKS_OPTIONALLY_OWNED_H
