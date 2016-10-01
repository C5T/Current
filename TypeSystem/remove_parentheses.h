/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_TYPE_SYSTEM_REMOVE_PARENTHESES_H
#define CURRENT_TYPE_SYSTEM_REMOVE_PARENTHESES_H

#include "../port.h"

#include <tuple>

// A macro to equally treat bare and parenthesized type arguments:
// REMOVE_PARENTHESES((int)) = REMOVE_PARENTHESES(int) = int
#define EMPTY_CF_TYPE_EXTRACT
#define CF_TYPE_PASTE(x, ...) x##__VA_ARGS__
#define CF_TYPE_PASTE2(x, ...) CF_TYPE_PASTE(x, __VA_ARGS__)
#define CF_TYPE_EXTRACT(...) CF_TYPE_EXTRACT __VA_ARGS__

// Was. -- D.K. TODO: Test on Windows.
// #define REMOVE_PARENTHESES(x) CF_TYPE_PASTE2(EMPTY_, CF_TYPE_EXTRACT x)

// Now. -- D.K.
#define REMOVE_PARENTHESES(...) CF_TYPE_PASTE2(EMPTY_, CF_TYPE_EXTRACT __VA_ARGS__)

// Self-test.
static int DIMA = 42;
static_assert(sizeof(is_same_or_compile_error<int, REMOVE_PARENTHESES(int)>), "");
static_assert(sizeof(is_same_or_compile_error<std::tuple<int>, std::tuple<REMOVE_PARENTHESES(int)>>), "");
static_assert(sizeof(is_same_or_compile_error<std::tuple<int, double>, std::tuple<REMOVE_PARENTHESES(int, double)>>), "");

#endif  // CURRENT_TYPE_SYSTEM_REMOVE_PARENTHESES_H
