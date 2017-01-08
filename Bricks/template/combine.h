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

#include "enable_if.h"
#include "typelist.h"
#include "weed.h"

namespace current {
namespace metaprogramming {

// `combine<TypeListImpl<A, B, C>>` == a `struct` that internally contains all `A`, `B` and `C`
// via "has-a" inheritance, and exposes `operator()`, calls to which are dispatched in compile time
// to the instances of `A`, `B` or `C` respectively, based on type signature.
// No matching invocation signature will result in compile-time error, same as the case
// of more than one invocation signature matching the call.
// For legacy reasons, `combine<std::tuple<...>>` is supported too.

template <typename T>
struct dispatch {
  T instance;

  template <typename... XS>
  ENABLE_IF<weed::call_with<T, XS...>::implemented, weed::call_with_type<T, XS...>> operator()(XS&&... params) {
    return instance(std::forward<XS>(params)...);
  }

  template <typename... XS>
  void DispatchToAll(XS&&... params) {
    return instance.DispatchToAll(std::forward<XS>(params)...);
  }
};

template <typename T1, typename T2>
struct inherit_from_both : T1, T2 {
  // `operator()(...)` is handled such that only one of `T1::operator()` and `T2::operator()` will match
  // the given type(s) of parameter(s) types. If none or both match, it's a compilation error.
  // `operator()(...)` is used to avoid various `using ...`-s throughout template metaprogramming code.
  using T1::operator();
  using T2::operator();

  // `DispatchToAll(...)` will reach both `T1` and `T2`.
  template <typename... TS>
  void DispatchToAll(TS&&... params) {
    T1::DispatchToAll(std::forward<TS>(params)...);
    T2::DispatchToAll(std::forward<TS>(params)...);
  }
};

template <typename T>
struct combine {};

template <typename T>
struct combine<std::tuple<T>> : dispatch<T> {};

template <typename T, typename... TS>
struct combine<std::tuple<T, TS...>> : inherit_from_both<dispatch<T>, combine<std::tuple<TS...>>> {};

template <typename T>
struct combine<TypeListImpl<T>> : dispatch<T> {};

template <typename T, typename... TS>
struct combine<TypeListImpl<T, TS...>> : inherit_from_both<dispatch<T>, combine<TypeListImpl<TS...>>> {};

// Support `DispatchToAll<TypeList[Impl]<>>`.
template <>
struct combine<TypeListImpl<>> {
  template <typename... XS>
  void DispatchToAll(XS&&...) {}
};

}  // namespace metaprogramming
}  // namespace current

#endif  // BRICKS_TEMPLATE_COMBINE_H
