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

#ifndef BRICKS_TEMPLATE_VARIADIC_INDEXES_H
#define BRICKS_TEMPLATE_VARIADIC_INDEXES_H

namespace bricks {
namespace variadic_indexes {

template <int...>
struct indexes {};
template <int X, int... XS>
struct indexes_generator : indexes_generator<X - 1, X - 1, XS...> {};
template <int... XS>
struct indexes_generator<0, XS...> {
  typedef indexes<XS...> type;
};

template<int N>
using generate_indexes = typename indexes_generator<N>::type;

}  // namespace variadic_indexes
}  // namespace bricks

#endif  // BRICKS_TEMPLATE_VARIADIC_INDEXES_H


