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

#ifndef BRICKS_TEMPLATE_WEED_H
#define BRICKS_TEMPLATE_WEED_H

#include "../../port.h"

// `weed::call_with<T, XS...>::implemented` => true or false depending on whether
// and instance of `T` can be invoked (`operator()`) with `XS...` as parameters.

// `weed::call_with_type<T, XS...>` is the return type of the above expression.

#include <utility>

namespace current {
namespace weed {

namespace impl {

template <typename T>
struct call_with_impl {
  template <typename... XS>
  static constexpr bool good_stuff(char) {
    return false;
  }

  template <typename... XS>
  static constexpr auto good_stuff(int) -> decltype(std::declval<T>()(std::declval<XS>()...), bool()) {
    return true;
  }
};

}  // namespace current::weed::impl

template <typename T, typename... XS>
struct call_with {
  enum { implemented = impl::call_with_impl<T>::template good_stuff<XS...>(0) };
};

template <typename T, typename... XS>
using call_with_type = decltype(std::declval<T>()(std::declval<XS>()...));

// Because it's good stuff.
namespace smoke_test {

struct TEST1 {
  struct A {};
  struct B {};
  void operator()(int) {}
  int operator()(A) { return 0; }
  int operator()(A, B) { return 0; }
};
struct TEST2 {};

static_assert(call_with<TEST1, int>::implemented, "");
static_assert(call_with<TEST1, TEST1::A>::implemented, "");
static_assert(!call_with<TEST1, TEST1::B>::implemented, "");
static_assert(call_with<TEST1, TEST1::A, TEST1::B>::implemented, "");

static_assert(!call_with<TEST2, int>::implemented, "");

}  // namespace smoke_test

}  // namespace current::weed
}  // namespace current

#endif  // BRICKS_TEMPLATE_WEED_H
