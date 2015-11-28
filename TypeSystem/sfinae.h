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

#ifndef CURRENT_TYPE_SYSTEM_SFINAE_H
#define CURRENT_TYPE_SYSTEM_SFINAE_H

#include <utility>

#include "../Bricks/template/enable_if.h"

namespace current {
namespace sfinae {

// Whether an `ExistsImpl()` method is defined for a type.
template <typename ENTRY>
constexpr bool HasExistsImplMethod(char) {
  return false;
}

template <typename ENTRY>
constexpr auto HasExistsImplMethod(int) -> decltype(std::declval<const ENTRY>().ExistsImpl(), bool()) {
  return true;
}

// Whether a `ValueImpl()` method is defined for a type.
template <typename ENTRY>
constexpr bool HasValueImplMethod(char) {
  return false;
}

template <typename ENTRY>
constexpr auto HasValueImplMethod(int) -> decltype(std::declval<const ENTRY>().ValueImpl(), bool()) {
  return true;
}

// Whether a `CloneImpl()` method is defined for a type.
template <typename ENTRY>
constexpr bool HasCloneImplMethod(char) {
  return false;
}

template <typename ENTRY>
constexpr auto HasCloneImplMethod(int) -> decltype(std::declval<const ENTRY>().CloneImpl(), bool()) {
  return true;
}

}  // namespace sfinae
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SFINAE_H
