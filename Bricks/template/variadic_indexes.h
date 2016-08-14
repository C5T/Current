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

namespace crnt {
namespace vi {

template <int...>
struct is {};
template <int X, int... XS>
struct ig : ig<X - 1, X - 1, XS...> {};
template <int... XS>
struct ig<0, XS...> {
  typedef is<XS...> type;
};

template <int N>
using gi = typename ig<N>::type;

}  // namespace vi
}  // namespace crnt

namespace current {
namespace variadic_indexes {

template <int... XS>
using indexes = ::crnt::vi::is<XS...>;

template <int... XS>
using indexes_generator = ::crnt::vi::ig<XS...>;

template <int N>
using generate_indexes = ::crnt::vi::gi<N>;

}  // namespace variadic_indexes
}  // namespace current

#endif  // BRICKS_TEMPLATE_VARIADIC_INDEXES_H
