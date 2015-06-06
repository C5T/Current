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

#ifndef BRICKS_TEMPLATE_DECAY_H
#define BRICKS_TEMPLATE_DECAY_H

#include <string>
#include <type_traits>

#include "mapreduce.h"

namespace bricks {

template <typename T>
struct rmref_impl {
  typedef typename std::remove_reference<T>::type type;
};

template <typename... TS>
struct rmref_impl<std::tuple<TS...>> {
  template <typename T>
  using impl = typename rmref_impl<T>::type;
  typedef metaprogramming::map<impl, std::tuple<TS...>> type;
};

template <typename... TS>
struct rmref_impl<const std::tuple<TS...>> {
  template <typename T>
  using impl = typename rmref_impl<T>::type;
  typedef metaprogramming::map<impl, std::tuple<TS...>> type;
};

template <typename... TS>
struct rmref_impl<std::tuple<TS...>&> {
  template <typename T>
  using impl = typename rmref_impl<T>::type;
  typedef metaprogramming::map<impl, std::tuple<TS...>> type;
};

template <typename... TS>
struct rmref_impl<const std::tuple<TS...>&> {
  template <typename T>
  using impl = typename rmref_impl<T>::type;
  typedef metaprogramming::map<impl, std::tuple<TS...>> type;
};

template <typename... TS>
struct rmref_impl<std::tuple<TS...>&&> {
  template <typename T>
  using impl = typename rmref_impl<T>::type;
  typedef metaprogramming::map<impl, std::tuple<TS...>> type;
};

template <typename... TS>
using rmref = typename rmref_impl<TS...>::type;

template <typename T>
struct rmconst_impl {
  typedef typename std::remove_const<T>::type type;
};

template <typename... TS>
struct rmconst_impl<std::tuple<TS...>> {
  template <typename T>
  using impl = typename rmconst_impl<T>::type;
  typedef metaprogramming::map<impl, std::tuple<TS...>> type;
};

template <typename... TS>
struct rmconst_impl<const std::tuple<TS...>> {
  template <typename T>
  using impl = typename rmconst_impl<T>::type;
  typedef metaprogramming::map<impl, std::tuple<TS...>> type;
};

template <typename... TS>
struct rmconst_impl<std::tuple<TS...>&> {
  template <typename T>
  using impl = typename rmconst_impl<T>::type;
  typedef metaprogramming::map<impl, std::tuple<TS...>> type;
};

template <typename... TS>
struct rmconst_impl<const std::tuple<TS...>&> {
  template <typename T>
  using impl = typename rmconst_impl<T>::type;
  typedef metaprogramming::map<impl, std::tuple<TS...>> type;
};

template <typename... TS>
struct rmconst_impl<std::tuple<TS...>&&> {
  template <typename T>
  using impl = typename rmconst_impl<T>::type;
  typedef metaprogramming::map<impl, std::tuple<TS...>> type;
};

template <typename... TS>
using rmconst = typename rmconst_impl<TS...>::type;

// Bricks define `decay<>` as the combination of `rmconst` and `rmref`, penetrating tuples.
// We did not need other functionality of `std::decay_t<>` yet. -- D.K.
template <typename... TS>
using decay = rmconst<rmref<TS...>>;

template <typename A, typename B>
using is_same = std::is_same<A, B>;

static_assert(is_same<int, rmref<int>>::value, "");
static_assert(is_same<int, rmref<int&>>::value, "");
static_assert(is_same<int, rmref<int&&>>::value, "");
static_assert(is_same<const int, rmref<const int>>::value, "");
static_assert(is_same<const int, rmref<const int&>>::value, "");
static_assert(is_same<const int, rmref<const int&&>>::value, "");

static_assert(is_same<std::string, rmconst<std::string>>::value, "");
static_assert(is_same<std::string, rmconst<const std::string>>::value, "");

static_assert(is_same<int, decay<int>>::value, "");
static_assert(is_same<int, decay<int&>>::value, "");
static_assert(is_same<int, decay<int&&>>::value, "");
static_assert(is_same<int, decay<const int>>::value, "");
static_assert(is_same<int, decay<const int&>>::value, "");
static_assert(is_same<int, decay<const int&&>>::value, "");

static_assert(is_same<std::string, decay<std::string>>::value, "");
static_assert(is_same<std::string, decay<std::string&>>::value, "");
static_assert(is_same<std::string, decay<std::string&&>>::value, "");
static_assert(is_same<std::string, decay<const std::string>>::value, "");
static_assert(is_same<std::string, decay<const std::string&>>::value, "");
static_assert(is_same<std::string, decay<const std::string&&>>::value, "");

static_assert(is_same<std::tuple<int, int, int, int>, decay<std::tuple<int, int, int, int>>>::value, "");
static_assert(
    is_same<std::tuple<int, int, int, int>, decay<std::tuple<const int, int&, const int&, int&&>>>::value, "");

static_assert(is_same<std::tuple<std::string>, decay<std::tuple<std::string>>>::value, "");
static_assert(is_same<std::tuple<std::string>, decay<std::tuple<const std::string>>>::value, "");
static_assert(is_same<std::tuple<std::string>, decay<std::tuple<const std::string&>>>::value, "");
static_assert(is_same<std::tuple<std::string>, decay<std::tuple<std::string&>>>::value, "");
static_assert(is_same<std::tuple<std::string>, decay<std::tuple<std::string&&>>>::value, "");

static_assert(is_same<std::tuple<std::string>, decay<const std::tuple<std::string>&>>::value, "");
static_assert(is_same<std::tuple<std::string>, decay<std::tuple<const std::string>>>::value, "");
static_assert(is_same<std::tuple<std::string>, decay<const std::tuple<const std::string>>>::value, "");
static_assert(is_same<std::tuple<std::string>, decay<const std::tuple<const std::string&>>>::value, "");
static_assert(is_same<std::tuple<std::string>, decay<const std::tuple<const std::string&>&>>::value, "");
static_assert(is_same<std::tuple<std::string>, decay<std::tuple<const std::string&>&&>>::value, "");

// YO DAWG! I HEARD YOU LIKE TUPLES!
static_assert(
    is_same<std::tuple<std::tuple<std::string>>, decay<std::tuple<std::tuple<const std::string>>>>::value, "");
static_assert(is_same<std::tuple<std::tuple<std::string>>, decay<std::tuple<std::tuple<std::string&>>>>::value,
              "");
static_assert(
    is_same<std::tuple<std::tuple<std::string>>, decay<std::tuple<std::tuple<const std::string&>>>>::value, "");
static_assert(is_same<std::tuple<std::tuple<std::string>>, decay<std::tuple<std::tuple<std::string&&>>>>::value,
              "");

static_assert(is_same<std::tuple<std::tuple<std::string>>, decay<std::tuple<std::tuple<std::string>>>>::value,
              "");
static_assert(
    is_same<std::tuple<std::tuple<std::string>>, decay<const std::tuple<std::tuple<std::string>>&>>::value, "");
static_assert(
    is_same<std::tuple<std::tuple<std::string>>, decay<const std::tuple<std::tuple<std::string>&>&>>::value,
    "");
static_assert(
    is_same<std::tuple<std::tuple<std::string>>, decay<const std::tuple<std::tuple<std::string&>&>&>>::value,
    "");
static_assert(
    is_same<std::tuple<std::tuple<std::string>>, decay<const std::tuple<std::tuple<std::string&&>&>&>>::value,
    "");

static_assert(is_same<std::tuple<std::tuple<int>, std::tuple<int>, std::tuple<int>, std::tuple<int>>,
                      decay<std::tuple<const std::tuple<const int>,
                                       std::tuple<int&>&,
                                       const std::tuple<const int&>&,
                                       std::tuple<int&&>&&>>>::value,
              "");

}  // namespace bricks

#endif  // BRICKS_TEMPLATE_DECAY_H
