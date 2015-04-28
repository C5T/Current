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

// Variadic methods. The convention is to use `std::tuple<>` for type lists.

#ifndef BRICKS_VARIADIC_VARIADIC_H
#define BRICKS_VARIADIC_VARIADIC_H

#include <tuple>

namespace bricks {
namespace variadic {

// `map<F<>, std::tuple<A, B, C>>` == `std::tuple<F<A>, F<B>, F<C>>`.

template <template <typename> class F, typename TS>
struct map_impl {};

template <template <typename> class F, typename TS>
using map = typename map_impl<F, TS>::type;

template <template <typename> class F, typename T>
struct map_impl<F, std::tuple<T>> {
  typedef std::tuple<F<T>> type;
};

template <template <typename> class F, typename T, typename... TS>
struct map_impl<F, std::tuple<T, TS...>> {
  typedef decltype(
      std::tuple_cat(std::declval<map<F, std::tuple<T>>>(), std::declval<map<F, std::tuple<TS...>>>())) type;
};


// `filter<F<>, std::tuple<A, B, C>>` == `std::tuple<>` that only contains types where `F<i>::filter` is `true`.

template <template <typename> class F, typename TS>
struct filter_impl {};

template <template <typename> class F, typename TS>
using filter = typename filter_impl<F, TS>::type;

template <template <typename> class F, typename T>
struct filter_impl<F, std::tuple<T>> {
  typedef typename std::conditional<F<T>::filter, std::tuple<T>, std::tuple<>>::type type;
};

template <template <typename> class F, typename T, typename... TS>
struct filter_impl<F, std::tuple<T, TS...>> {
  typedef decltype(
      std::tuple_cat(std::declval<filter<F, std::tuple<T>>>(), std::declval<filter<F, std::tuple<TS...>>>())) type;
};


// `reduce<F<LHS, RHS>, std::tuple<A, B, C>>` == `F<A, F<B, C>>`.

template <template <typename, typename> class F, typename TS>
struct reduce_impl {};

template <template <typename, typename> class F, typename TS>
using reduce = typename reduce_impl<F, TS>::type;

template <template <typename, typename> class F, typename T>
struct reduce_impl<F, std::tuple<T>> {
  typedef T type;
};

template <template <typename, typename> class F, typename T, typename... TS>
struct reduce_impl<F, std::tuple<T, TS...>> {
  typedef F<T, reduce<F, std::tuple<TS...>>> type;
};


// `combine<std::tuple<A, B, C>>` == a `struct` that combines all members and fields from `A`, `B` and `C`.

template<typename A, typename B>
struct multiple_inheritance_combiner_impl {
  struct type : A, B {};
};

template<typename A, typename B>
using multiple_inheritance_combiner = typename multiple_inheritance_combiner_impl<A, B>::type;

template <typename TS>
using combine = reduce<multiple_inheritance_combiner, TS>;

}  // namespace variadic
}  // namespace bricks

#endif  // BRICKS_VARIADIC_VARIADIC_H
