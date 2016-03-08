/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_STORAGE_REST_SFINAE_H
#define CURRENT_STORAGE_REST_SFINAE_H

#include "../storage.h"

namespace current {
namespace storage {
namespace rest {
namespace sfinae {

template <typename T>
constexpr bool Has_T_BRIEF(char) {
  return false;
}

template <typename T>
constexpr auto Has_T_BRIEF(int) -> decltype(sizeof(typename T::T_BRIEF), bool()) {
  return true;
}

template <typename T, bool>
struct BRIEF_OF_T_IMPL;

template <typename T>
struct BRIEF_OF_T_IMPL<T, false> {
  using type = T;
};

template <typename T>
struct BRIEF_OF_T_IMPL<T, true> {
  using type = typename T::T_BRIEF;
};

template <typename T>
using BRIEF_OF_T = typename BRIEF_OF_T_IMPL<T, Has_T_BRIEF<T>(0)>::type;

}  // namespace sfinae
}  // namespace rest
}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_REST_SFINAE_H
