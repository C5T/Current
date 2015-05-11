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

#ifndef BRICKS_TEMPLATE_COMBINE_H
#define BRICKS_TEMPLATE_COMBINE_H

#include <tuple>
#include <utility>

namespace bricks {
namespace metaprogramming {

// `combine<std::tuple<A, B, C>>` == a `struct` that internally contains all `A`, `B` and `C`
// via "has-a" inheritance, and exposes `operator()`, calls to which are dispatched in compile time
// to the instances of `A`, `B` or `C` respectively, based on type signature.
// No matching invocation signature will result in compile-time error, same as the case
// of more than one invocation signature matching the call.

template <typename T>
struct dispatch {
  T instance;

  template <typename... XS>
  static constexpr bool sfinae(char) {
    return false;
  }

  template <typename... XS>
  static constexpr auto sfinae(int) -> decltype(std::declval<T>()(std::declval<XS>()...), bool()) {
    return true;
  }

  template <typename... XS>
  typename std::enable_if<sfinae<XS...>(0), decltype(std::declval<T>()(std::declval<XS>()...))>::type
  operator()(XS... params) {
    return instance(params...);
  }
};

template <typename T1, typename T2>
struct inherit_from_both : T1, T2 {
  using T1::operator();
  using T2::operator();
  // Note: clang++ passes `operator`-s through
  // by default, whereas g++ requires `using`-s. --  D.K.
};

template <typename T>
struct combine {};

template <typename T>
struct combine<std::tuple<T>> : dispatch<T> {};

template <typename T, typename... TS>
struct combine<std::tuple<T, TS...>> : inherit_from_both<dispatch<T>, combine<std::tuple<TS...>>> {};

}  // namespace metaprogramming
}  // namespace bricks

#endif  // BRICKS_TEMPLATE_COMBINE_H
