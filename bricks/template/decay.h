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

#include "../../port.h"

#include <string>
#include <type_traits>

#include "mapreduce.h"

namespace current {

template <typename DERIVED, typename BASE>
inline DERIVED* dynamic_cast_preserving_const(BASE* base) {
  return dynamic_cast<DERIVED*>(base);
}

template <typename DERIVED, typename BASE>
inline const DERIVED* dynamic_cast_preserving_const(const BASE* base) {
  return dynamic_cast<const DERIVED*>(base);
}

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

template <typename T>
using rmref = typename rmref_impl<T>::type;

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

template <typename T>
using rmconst = typename rmconst_impl<T>::type;

// Bricks define `decay<>` as the combination of `rmconst` and `rmref`, penetrating tuples.
// We did not need other functionality of `std::decay_t<>` yet. -- D.K.
template <typename T>
using decay = rmconst<rmref<T>>;

template <typename A, typename B>
inline constexpr bool is_same_v = std::is_same_v<A, B>;

static_assert(is_same_v<int, rmref<int>>, "");
static_assert(is_same_v<int, rmref<int&>>, "");
static_assert(is_same_v<int, rmref<int&&>>, "");
static_assert(is_same_v<const int, rmref<const int>>, "");
static_assert(is_same_v<const int, rmref<const int&>>, "");
static_assert(is_same_v<const int, rmref<const int&&>>, "");

static_assert(is_same_v<std::string, rmconst<std::string>>, "");
static_assert(is_same_v<std::string, rmconst<const std::string>>, "");

static_assert(is_same_v<int, decay<int>>, "");
static_assert(is_same_v<int, decay<int&>>, "");
static_assert(is_same_v<int, decay<int&&>>, "");
static_assert(is_same_v<int, decay<const int>>, "");
static_assert(is_same_v<int, decay<const int&>>, "");
static_assert(is_same_v<int, decay<const int&&>>, "");

static_assert(is_same_v<std::string, decay<std::string>>, "");
static_assert(is_same_v<std::string, decay<std::string&>>, "");
static_assert(is_same_v<std::string, decay<std::string&&>>, "");
static_assert(is_same_v<std::string, decay<const std::string>>, "");
static_assert(is_same_v<std::string, decay<const std::string&>>, "");
static_assert(is_same_v<std::string, decay<const std::string&&>>, "");

static_assert(is_same_v<std::tuple<int, int, int, int>, decay<std::tuple<int, int, int, int>>>, "");
static_assert(is_same_v<std::tuple<int, int, int, int>, decay<std::tuple<const int, int&, const int&, int&&>>>,
              "");

static_assert(is_same_v<std::tuple<std::string>, decay<std::tuple<std::string>>>, "");
static_assert(is_same_v<std::tuple<std::string>, decay<std::tuple<const std::string>>>, "");
static_assert(is_same_v<std::tuple<std::string>, decay<std::tuple<const std::string&>>>, "");
static_assert(is_same_v<std::tuple<std::string>, decay<std::tuple<std::string&>>>, "");
static_assert(is_same_v<std::tuple<std::string>, decay<std::tuple<std::string&&>>>, "");

static_assert(is_same_v<std::tuple<std::string>, decay<const std::tuple<std::string>&>>, "");
static_assert(is_same_v<std::tuple<std::string>, decay<std::tuple<const std::string>>>, "");
static_assert(is_same_v<std::tuple<std::string>, decay<const std::tuple<const std::string>>>, "");
static_assert(is_same_v<std::tuple<std::string>, decay<const std::tuple<const std::string&>>>, "");
static_assert(is_same_v<std::tuple<std::string>, decay<const std::tuple<const std::string&>&>>, "");
static_assert(is_same_v<std::tuple<std::string>, decay<std::tuple<const std::string&>&&>>, "");

static_assert(is_same_v<char[5], decay<decltype("test")>>, "");

// YO DAWG! I HEARD YOU LIKE TUPLES!
static_assert(is_same_v<std::tuple<std::tuple<std::string>>, decay<std::tuple<std::tuple<const std::string>>>>,
              "");
static_assert(is_same_v<std::tuple<std::tuple<std::string>>, decay<std::tuple<std::tuple<std::string&>>>>, "");
static_assert(is_same_v<std::tuple<std::tuple<std::string>>, decay<std::tuple<std::tuple<const std::string&>>>>,
              "");
static_assert(is_same_v<std::tuple<std::tuple<std::string>>, decay<std::tuple<std::tuple<std::string&&>>>>, "");
static_assert(is_same_v<std::tuple<std::tuple<std::string>>, decay<std::tuple<std::tuple<std::string>>>>, "");
static_assert(is_same_v<std::tuple<std::tuple<std::string>>, decay<const std::tuple<std::tuple<std::string>>&>>,
              "");
static_assert(is_same_v<std::tuple<std::tuple<std::string>>, decay<const std::tuple<std::tuple<std::string>&>&>>,
              "");
static_assert(is_same_v<std::tuple<std::tuple<std::string>>, decay<const std::tuple<std::tuple<std::string&>&>&>>,
              "");
static_assert(is_same_v<std::tuple<std::tuple<std::string>>, decay<const std::tuple<std::tuple<std::string&&>&>&>>,
              "");
static_assert(is_same_v<std::tuple<std::tuple<int>, std::tuple<int>, std::tuple<int>, std::tuple<int>>,
                        decay<std::tuple<const std::tuple<const int>,
                                         std::tuple<int&>&,
                                         const std::tuple<const int&>&,
                                         std::tuple<int&&>&&>>>,
              "");

// `current::decay<>` is different from `std::decay<>`, as pointers and some `const`-ness are kept. -- D.K.
static_assert(is_same_v<char*, decay<char*>>, "");
static_assert(is_same_v<char*, decay<char*&>>, "");
static_assert(is_same_v<char*, decay<char*&&>>, "");
static_assert(is_same_v<const char*, decay<const char*>>, "");
static_assert(is_same_v<const char*, decay<char const*>>, "");
static_assert(is_same_v<char*, decay<char* const>>, "");
static_assert(is_same_v<const char*, decay<const char*&>>, "");
static_assert(is_same_v<const char*, decay<char const*&>>, "");
static_assert(is_same_v<char*, decay<char* const&>>, "");
static_assert(is_same_v<const char*, decay<const char*&&>>, "");
static_assert(is_same_v<const char*, decay<char const*&&>>, "");
static_assert(is_same_v<char*, decay<char* const&&>>, "");

}  // namespace current

#endif  // BRICKS_TEMPLATE_DECAY_H
