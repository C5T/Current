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

#ifndef CURRENT_TYPE_SYSTEM_HELPERS_H
#define CURRENT_TYPE_SYSTEM_HELPERS_H

#include "sfinae.h"

namespace current {

template <typename T>
struct ExistsImpl {
  // Primitive types.
  template <typename TT = T>
  static ENABLE_IF<!current::sfinae::HasExistsMethod<TT>(0), bool> CallExists(TT&&) {
    return true;
  }

  // Special types.
  template <typename TT = T>
  static ENABLE_IF<current::sfinae::HasExistsMethod<TT>(0), bool> CallExists(TT&& x) {
    return x.Exists();
  }
};

template <typename T>
bool Exists(T&& x) {
  return ExistsImpl<T>::CallExists(std::forward<T>(x));
}

template <typename T>
struct ValueImpl {
  // Primitive types.
  template <typename TT = T>
  static ENABLE_IF<!current::sfinae::HasValueMethod<TT>(0), T&&> CallValue(T&& x) {
    return x;
  }

  // Special types.
  template <typename TT = T>
  static ENABLE_IF<current::sfinae::HasValueMethod<TT>(0), decltype(std::declval<TT>().Value())> CallValue(
      TT&& x) {
    return x.Value();
  }
};

template <typename T>
auto Value(T&& x) -> decltype(ValueImpl<T>::CallValue(std::declval<T>())) {
  return ValueImpl<T>::CallValue(std::forward<T>(x));
}

}  // namespace current

using current::Exists;
using current::Value;

#endif  // CURRENT_TYPE_SYSTEM_HELPERS_H
