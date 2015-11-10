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

#ifndef BRICKS_UTIL_CLONE_H
#define BRICKS_UTIL_CLONE_H

#include <functional>

#include "../cerealize/json.h"

namespace bricks {
namespace impl {

template <typename T, bool HAS_CLONE_BY_REF, bool HAS_CLONE_BY_PTR, bool HAS_COPY_CONSTRUCTOR>
struct DefaultCloneImpl {};

template <typename T, bool HAS_CLONE_BY_PTR, bool HAS_COPY_CONSTRUCTOR>
struct DefaultCloneImpl<T, true, HAS_CLONE_BY_PTR, HAS_COPY_CONSTRUCTOR> {
  static std::function<T(const T&)> CloneImpl() {
    return [](const T& t) { return t.Clone(); };
  }
};

template <typename T, bool HAS_COPY_CONSTRUCTOR>
struct DefaultCloneImpl<T, false, true, HAS_COPY_CONSTRUCTOR> {
  static std::function<T(const T&)> CloneImpl() {
    return [](const T& t) { return t->Clone(); };
  }
};

template <typename T>
struct DefaultCloneImpl<T, false, false, true> {
  static std::function<T(const T&)> CloneImpl() {
    return [](const T& t) { return t; };
  }
};

template <typename T>
struct DefaultCloneImpl<T, false, false, false> {
  static std::function<T(const T&)> CloneImpl() {
    return [](const T& t) { return CerealizeParseJSON<T>(CerealizeJSON(t)); };
  }
};

template <typename T>
constexpr bool HasCloneByRef(char) {
  return false;
}

template <typename T>
constexpr auto HasCloneByRef(int) -> decltype(std::declval<const T&>().Clone(), bool()) {
  return true;
}

template <typename T>
constexpr bool HasCloneByPtr(char) {
  return false;
}

template <typename T>
constexpr auto HasCloneByPtr(int) -> decltype(std::declval<const T&>()->Clone(), bool()) {
  return true;
}

template <typename T>
constexpr bool HasCopyConstructor(char) {
  return false;
}

template <typename T>
constexpr auto HasCopyConstructor(int) -> decltype(T(std::declval<const T&>()), bool()) {
  return true;
}

}  // namespace bricks::impl

// A wrapper returning an `std::function<>`.
template <typename T>
std::function<T(const T&)> DefaultCloneFunction() {
  return impl::DefaultCloneImpl<T,
                                impl::HasCloneByRef<T>(0),
                                impl::HasCloneByPtr<T>(0),
                                impl::HasCopyConstructor<T>(0)>::CloneImpl();
}

// A top-level `Clone()` method.
template <typename T>
T Clone(const T& object) {
  return DefaultCloneFunction<T>()(object);
}

// A templated version of the above, to allow compile-time cloner specification
// without having to inject and/or carry over an `std::function<>` performing the `Clone()`.
struct DefaultCloner {
  template <typename T>
  static T Clone(const T& input) {
    return bricks::Clone(input);
  }
};

}  // namespace bricks

#endif  // BRICKS_UTIL_CLONE_H
