/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_TEMPLATE_CALL_ALL_CONSTRUCTORS_H
#define BRICKS_TEMPLATE_CALL_ALL_CONSTRUCTORS_H

#include "typelist.h"

namespace current {
namespace metaprogramming {

template <template <class> class MAPPER, class TYPELIST>
struct call_all_constructors_impl;

template <template <class> class MAPPER, class... TS>
struct call_all_constructors_impl<MAPPER, TypeListImpl<TS...>> : MAPPER<TS>... {};

template <template <class> class MAPPER, class TYPELIST>
void call_all_constructors() {
  call_all_constructors_impl<MAPPER, TYPELIST> impl;
}

template <template <class> class MAPPER, class OBJECT, class TYPELIST>
struct call_all_constructors_with_impl;

template <template <class> class MAPPER, class OBJECT, class... TS>
struct call_all_constructors_with_impl<MAPPER, OBJECT, TypeListImpl<TS...>> : MAPPER<TS>... {
  call_all_constructors_with_impl(OBJECT& object) : MAPPER<TS>(object)... {}
};

template <template <class> class MAPPER, class OBJECT, class TYPELIST>
void call_all_constructors_with(OBJECT& object) {
  call_all_constructors_with_impl<MAPPER, OBJECT, TYPELIST> impl(object);
}

}  // namespace metaprogramming
}  // namespace current

#endif  // BRICKS_TEMPLATE_CALL_ALL_CONSTRUCTORS_H
