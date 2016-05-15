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

#ifndef BRICKS_TEMPLATE_VISITOR_H
#define BRICKS_TEMPLATE_VISITOR_H

#ifndef CURRENT_MAKE_CHECK_MODE
#error "`visitor.h` is being deprecated now."
#endif  // CURRENT_MAKE_CHECK_MODE

#if 0

#include <tuple>

#include "is_tuple.h"

namespace current {
namespace metaprogramming {

// Library code.

template <typename>
struct visitor {};

template <>
struct visitor<std::tuple<>> {};

template <typename T>
struct virtual_visit_method {
  virtual void visit(T&) = 0;
};

template <typename T, typename... TS>
struct visitor<std::tuple<T, TS...>> : visitor<std::tuple<TS...>>, virtual_visit_method<T> {};

template <typename TYPELIST_AS_TUPLE>
struct abstract_visitable {
  static_assert(is_std_tuple<TYPELIST_AS_TUPLE>::value, "");
  virtual void accept(visitor<TYPELIST_AS_TUPLE>&) = 0;
};

template <typename TYPELIST_AS_TUPLE, typename T>
struct visitable : abstract_visitable<TYPELIST_AS_TUPLE> {
  static_assert(is_std_tuple<TYPELIST_AS_TUPLE>::value, "");
  virtual void accept(visitor<TYPELIST_AS_TUPLE>& v) override {
    static_cast<virtual_visit_method<T>&>(v).visit(*static_cast<T*>(this));
  }
};

}  // namespace metaprogramming
}  // namespace current

#endif

#endif  // BRICKS_TEMPLATE_VISITOR_H
