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

namespace crnt {

// The superclass for all Current-defined types, to enable polymorphic serialization and deserialization.
struct CurrentSuper {
  virtual ~CurrentSuper() = default;
};

// For `unique_ptr<>`-s.
struct CurrentSuperDeleter {
  void operator()(CurrentSuper* ptr) { delete ptr; }
};

// The superclass for all Current structures.
struct CurrentStruct : CurrentSuper {};

// The superclass for `Variant` type.
struct CurrentVariant : CurrentSuper {};

#define IS_CURRENT_STRUCT(T) (std::is_base_of<::crnt::CurrentStruct, ::current::decay<T>>::value)
#define IS_CURRENT_VARIANT(T) (std::is_base_of<::crnt::CurrentVariant, ::current::decay<T>>::value)
#define IS_CURRENT_STRUCT_OR_VARIANT(T) (std::is_base_of<::crnt::CurrentSuper, ::current::decay<T>>::value)

#define IS_EMPTY_CURRENT_STRUCT(T) (::current::reflection::IsEmptyCurrentStruct<T, IS_CURRENT_STRUCT(T)>::value)

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

template <typename ENTRY>
struct ExistsImplMethodTest {
  static constexpr bool value = HasExistsImplMethod<ENTRY>(0);
};

// Whether a `ValueImpl()` method is defined for a type.
template <typename ENTRY>
constexpr bool HasValueImplMethod(char) {
  return false;
}

template <typename ENTRY>
constexpr auto HasValueImplMethod(int) -> decltype(std::declval<const ENTRY>().ValueImpl(), bool()) {
  return true;
}

template <typename ENTRY>
struct ValueImplMethodTest {
  static constexpr bool value = HasValueImplMethod<ENTRY>(0);
};

// Whether a `SuccessfulImpl()` method is defined for a type.
template <typename ENTRY>
constexpr bool HasSuccessfulImplMethod(char) {
  return false;
}

template <typename ENTRY>
constexpr auto HasSuccessfulImplMethod(int) -> decltype(std::declval<const ENTRY>().SuccessfulImpl(), bool()) {
  return true;
}

template <typename ENTRY>
struct SuccessfulImplMethodTest {
  static constexpr bool value = HasSuccessfulImplMethod<ENTRY>(0);
};

// Whether an `CheckIntegrityImpl()` method is defined for a type.
template <typename ENTRY>
constexpr bool HasCheckIntegrityImplMethod(char) {
  return false;
}

template <typename ENTRY>
constexpr auto HasCheckIntegrityImplMethod(int) -> decltype(std::declval<const ENTRY>().CheckIntegrityImpl(), bool()) {
  return true;
}

template <typename ENTRY>
struct CheckIntegrityImplMethodTest {
  static constexpr bool value = HasCheckIntegrityImplMethod<ENTRY>(0);
};

// Whether an `CURRENT_EXPORTED_STRUCT_NAME()` method is defined for a type.
template <typename ENTRY>
constexpr bool Has_CURRENT_EXPORTED_STRUCT_NAME_Impl(char) {
  return false;
}

template <typename ENTRY>
constexpr auto Has_CURRENT_EXPORTED_STRUCT_NAME_Impl(int)
    -> decltype(std::declval<const ENTRY>().CURRENT_EXPORTED_STRUCT_NAME(), bool()) {
  return true;
}

template <typename ENTRY>
struct Has_CURRENT_EXPORTED_STRUCT_NAME {
  static constexpr bool value = Has_CURRENT_EXPORTED_STRUCT_NAME_Impl<ENTRY>(0);
};

}  // namespace crnt::sfinae
}  // namespace crnt

namespace current {
using ::crnt::CurrentSuper;
using ::crnt::CurrentSuperDeleter;
using ::crnt::CurrentStruct;
using ::crnt::CurrentVariant;
namespace sfinae {
using ::crnt::sfinae::HasExistsImplMethod;
using ::crnt::sfinae::ValueImplMethodTest;
using ::crnt::sfinae::HasCheckIntegrityImplMethod;
using ::crnt::sfinae::Has_CURRENT_EXPORTED_STRUCT_NAME;
}  // namespace current::sfinae
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SFINAE_H
