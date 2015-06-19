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

#ifndef BRICKS_TEMPLATE_IS_UNIQUE_PTR_H
#define BRICKS_TEMPLATE_IS_UNIQUE_PTR_H

#include <memory>

#include "decay.h"

namespace bricks {

template <typename T>
struct is_unique_ptr_impl {
  enum { value = false };
  typedef T underlying_type;
  // TODO(dkorolev): Universal or const version?
  static T& extract(T& x) { return x; }
};

template <typename T, typename D>
struct is_unique_ptr_impl<std::unique_ptr<T, D>> {
  enum { value = true };
  typedef T underlying_type;
  // TODO(dkorolev): Universal or const version?
  static T& extract(std::unique_ptr<T, D>& x) { return *x.get(); }
};

template <typename T>
using is_unique_ptr = is_unique_ptr_impl<decay<T>>;

static_assert(!is_unique_ptr<int>::value, "");
static_assert(std::is_same<int, typename is_unique_ptr<int>::underlying_type>::value, "");

static_assert(is_unique_ptr<std::unique_ptr<int>>::value, "");
static_assert(std::is_same<int, typename is_unique_ptr<std::unique_ptr<int>>::underlying_type>::value, "");

template <typename B, typename E>
struct can_be_stored_in_unique_ptr {
  static constexpr bool value = false;
};

// Note: No custom deleters here [yet]. TODO(dkorolev): Make sure they are here as we need them.
template <typename B, typename E>
struct can_be_stored_in_unique_ptr<std::unique_ptr<B>, E> {
  static constexpr bool value = std::is_base_of<B, E>::value;
};

namespace selftest {

struct BASE {
  virtual ~BASE() = default;
};

struct DERIVED : BASE {};

struct STANDALONE {};

static_assert(can_be_stored_in_unique_ptr<std::unique_ptr<BASE>, BASE>::value, "");
static_assert(can_be_stored_in_unique_ptr<std::unique_ptr<BASE>, DERIVED>::value, "");
static_assert(!can_be_stored_in_unique_ptr<std::unique_ptr<BASE>, STANDALONE>::value, "");

static_assert(!can_be_stored_in_unique_ptr<std::unique_ptr<DERIVED>, BASE>::value, "");
static_assert(can_be_stored_in_unique_ptr<std::unique_ptr<DERIVED>, DERIVED>::value, "");
static_assert(!can_be_stored_in_unique_ptr<std::unique_ptr<BASE>, STANDALONE>::value, "");

static_assert(!can_be_stored_in_unique_ptr<std::unique_ptr<BASE>, std::unique_ptr<BASE>>::value, "");
static_assert(!can_be_stored_in_unique_ptr<std::unique_ptr<BASE>, std::unique_ptr<DERIVED>>::value, "");
static_assert(!can_be_stored_in_unique_ptr<std::unique_ptr<DERIVED>, std::unique_ptr<DERIVED>>::value, "");

}  // namespace bricks::selftest

}  // namespace bricks

#endif  // BRICKS_TEMPLATE_IS_UNIQUE_PTR_H
