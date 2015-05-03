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

#ifndef BRICKS_VARIADIC_RMREF_H
#define BRICKS_VARIADIC_RMREF_H

#include <string>
#include <type_traits>

namespace bricks {

template <typename T>
using rmref = typename std::remove_reference<T>::type;

template <typename T>
using rmconst = typename std::remove_const<T>::type;

template <typename T>
using rmconstref = rmconst<rmref<T>>;

static_assert(std::is_same<int, rmref<int>>::value, "");
static_assert(std::is_same<int, rmref<int&>>::value, "");
static_assert(std::is_same<int, rmref<int&&>>::value, "");
static_assert(std::is_same<const int, rmref<const int>>::value, "");
static_assert(std::is_same<const int, rmref<const int&>>::value, "");
static_assert(std::is_same<const int, rmref<const int&&>>::value, "");

static_assert(std::is_same<std::string, rmconst<std::string>>::value, "");
static_assert(std::is_same<std::string, rmconst<const std::string>>::value, "");

static_assert(std::is_same<int, rmconstref<int>>::value, "");
static_assert(std::is_same<int, rmconstref<int&>>::value, "");
static_assert(std::is_same<int, rmconstref<int&&>>::value, "");
static_assert(std::is_same<int, rmconstref<const int>>::value, "");
static_assert(std::is_same<int, rmconstref<const int&>>::value, "");
static_assert(std::is_same<int, rmconstref<const int&&>>::value, "");

static_assert(std::is_same<std::string, rmconstref<std::string>>::value, "");
static_assert(std::is_same<std::string, rmconstref<std::string&>>::value, "");
static_assert(std::is_same<std::string, rmconstref<std::string&&>>::value, "");
static_assert(std::is_same<std::string, rmconstref<const std::string>>::value, "");
static_assert(std::is_same<std::string, rmconstref<const std::string&>>::value, "");
static_assert(std::is_same<std::string, rmconstref<const std::string&&>>::value, "");

}  // namespace bricks

#endif  // BRICKS_VARIADIC_RMREF_H
