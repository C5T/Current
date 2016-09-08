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

#ifndef BRICKS_TEMPLATE_MAPREDUCE_H
#define BRICKS_TEMPLATE_MAPREDUCE_H

#include <cstdlib>
#include <tuple>

#include "typelist.h"

namespace current {
namespace metaprogramming {

// `map<F<>, std::tuple<A, B, C>>` == `std::tuple<F<A>, F<B>, F<C>>`.
template <template <typename> class F, typename TS>
struct map_impl {};

template <template <typename> class F, typename TS>
using map = typename map_impl<F, TS>::type;

template <template <typename> class F>
struct map_impl<F, std::tuple<>> {
  typedef std::tuple<> type;
};

template <template <typename> class F, typename... TS>
struct map_impl<F, std::tuple<TS...>> {
  typedef std::tuple<F<TS>...> type;
};

template <template <typename> class F>
struct map_impl<F, TypeListImpl<>> {
  typedef TypeListImpl<> type;
};

template <template <typename> class F, typename... TS>
struct map_impl<F, TypeListImpl<TS...>> {
  typedef TypeListImpl<F<TS>...> type;
};

// `filter<F<>, std::tuple<A, B, C>>` == `std::tuple<>` that only contains types where `F<i>::filter` is `true`.
template <template <typename> class F, typename RESULT_SO_FAR, typename TS>
struct filter_impl {};

template <template <typename> class F, typename... RESULT_SO_FAR>
struct filter_impl<F, std::tuple<RESULT_SO_FAR...>, std::tuple<>> {
  using result = std::tuple<RESULT_SO_FAR...>;
};

template <template <typename> class F, typename... RESULT_SO_FAR, typename T, typename... TS>
struct filter_impl<F, std::tuple<RESULT_SO_FAR...>, std::tuple<T, TS...>> {
  using result = typename filter_impl<
      F,
      typename std::conditional<F<T>::filter, std::tuple<RESULT_SO_FAR..., T>, std::tuple<RESULT_SO_FAR...>>::type,
      std::tuple<TS...>>::result;
};

template <template <typename> class F, typename... RESULT_SO_FAR>
struct filter_impl<F, TypeListImpl<RESULT_SO_FAR...>, TypeListImpl<>> {
  using result = TypeListImpl<RESULT_SO_FAR...>;
};

template <template <typename> class F, typename... RESULT_SO_FAR, typename T, typename... TS>
struct filter_impl<F, TypeListImpl<RESULT_SO_FAR...>, TypeListImpl<T, TS...>> {
  using result = typename filter_impl<
      F,
      typename std::conditional<F<T>::filter, TypeListImpl<RESULT_SO_FAR..., T>, TypeListImpl<RESULT_SO_FAR...>>::type,
      TypeListImpl<TS...>>::result;
};

template <template <typename> class, typename T>
struct filter_impl_selector;

template <template <typename> class F, typename... TS>
struct filter_impl_selector<F, std::tuple<TS...>> {
  using type = typename filter_impl<F, std::tuple<>, std::tuple<TS...>>::result;
};

template <template <typename> class F, typename... TS>
struct filter_impl_selector<F, TypeListImpl<TS...>> {
  using type = typename filter_impl<F, TypeListImpl<>, TypeListImpl<TS...>>::result;
};

template <template <typename> class F, typename T>
using filter = typename filter_impl_selector<F, T>::type;

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

template <template <typename, typename> class F, typename T>
struct reduce_impl<F, TypeListImpl<T>> {
  typedef T type;
};

template <template <typename, typename> class F, typename T, typename... TS>
struct reduce_impl<F, TypeListImpl<T, TS...>> {
  typedef F<T, reduce<F, TypeListImpl<TS...>>> type;
};

}  // namespace metaprogramming
}  // namespace current

#endif  // BRICKS_TEMPLATE_MAPREDUCE_H
