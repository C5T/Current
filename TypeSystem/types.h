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

#include "../port.h"

#include <map>
#include <memory>
#include <utility>

#include "../Bricks/template/pod.h"
#include "../Bricks/template/enable_if.h"

namespace current {

// The superclass for all Current-defined types, to enable polymorphic serialization and deserialization.
struct CurrentSuper {
  virtual ~CurrentSuper() = default;
};

#define IS_CURRENT_STRUCT(T) (std::is_base_of<::current::CurrentSuper, T>::value)

template <typename TYPELIST>
struct PolymorphicImpl;

template <typename T>
struct IS_POLYMORPHIC {
  enum { value = false };
};

template <typename TYPELIST>
struct IS_POLYMORPHIC<PolymorphicImpl<TYPELIST>> {
  enum { value = true };
};

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

// Whether an `CheckIntegrityImpl()` method is defined for a type.
template <typename ENTRY>
constexpr bool HasCheckIntegrityImplMethod(char) {
  return false;
}

template <typename ENTRY>
constexpr auto HasCheckIntegrityImplMethod(int)
    -> decltype(std::declval<const ENTRY>().CheckIntegrityImpl(), bool()) {
  return true;
}

}  // namespace sfinae
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SFINAE_H
