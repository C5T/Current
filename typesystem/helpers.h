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

// Current-friendly types can always be `std::move()`-d.
//
// For optional and/or polymorphic types, use the global `Exists()` and `Value()` functions.
//
// * bool Exists(x)      : non-throwing, always `true` for non-optional types.
// * [const] T& Value(x) : returns the value, throws `NoValue` if and only if `Exists(x)` is `false`.
//
// For primitive types, `Exists()` is always `true`, and `Value(x)` is equivalent to `x`.
// For types that can be optional, `Exists()` may be `false`, in which cases `Value(x)` will throw.
//
// Other two powerful syntaxes of `Exists(x)` and `Value(x)` deal with polymorphic types:
//
// * bool Exists<T>(x)      : non-throwing, returns `true` if and only of `x` can be seen as an instance of `T`.
// * [const] T& Value<T>(x) : returns the value, throws `NoValue` if and only if `Exists<T>(x)` is false.
//
// The exception type is derived from `NoValueException`, alias as `NoValue = const NoValueException&`.
// Catching on this top-level type is type-safe. Alternatively, a type-specific syntax
// of `catch (NoValueOfType<T>) { ... }` can be used for finer granularity.
//
// For polymorphic types, Exists<Base>(derived) is true, and Value<Base>(derived) is intentionally supported.
// Use `polymorphic.Call(functor)` for perfect type dispatching.

#ifndef CURRENT_TYPE_SYSTEM_HELPERS_H
#define CURRENT_TYPE_SYSTEM_HELPERS_H

#include "../port.h"

#include <memory>

#include "types.h"
#include "typename.h"

#include "../bricks/template/decay.h"

namespace current {

template <typename TEST, typename T, bool BOOL_IS_VARIANT>
struct ExistsImplCallerRespectingVariant;

template <typename TEST, typename T>
struct ExistsImplCallerRespectingVariant<TEST, T, true> {
  template <typename TT = T>
  static bool CallExistsImpl(TT&& x) {
    return x.template VariantExistsImpl<TEST>();
  }
};

template <typename TEST, typename T>
struct ExistsImplCallerRespectingVariant<TEST, T, false> {
  template <typename TT = T>
  static bool CallExistsImpl(TT&&) {
    return false;
  }
};

template <typename TEST, typename T>
struct ExistsImplCaller {
  template <typename TT = T>
  static bool CallExistsImpl(TT&& x) {
    return ExistsImplCallerRespectingVariant<TEST, T, IS_CURRENT_VARIANT(TT)>::CallExistsImpl(std::forward<TT>(x));
  }
};

// MSVS is not friendly with `std::enable_if_t` as return type, but OK with it as a template parameter. -- D.K.
template <typename T>
struct ExistsImplCaller<T, T> {
  // Primitive types.
  template <typename TT = T, class = std::enable_if_t<!sfinae::HasExistsImplMethod<TT>(0)>>
  static bool CallExistsImpl(TT&&) {
    return true;
  }

  // Special types.
  template <typename TT = T, class = std::enable_if_t<sfinae::HasExistsImplMethod<TT>(0)>>
  static std::enable_if_t<sfinae::HasExistsImplMethod<TT>(0), bool> CallExistsImpl(TT&& x) {
    return x.ExistsImpl();
  }
};

struct DefaultExistsInvocation {};
template <typename TEST = DefaultExistsInvocation, typename T>
bool Exists(T&& x) {
  return ExistsImplCaller<
      typename std::conditional<std::is_same<TEST, DefaultExistsInvocation>::value, current::decay<T>, TEST>::type,
      current::decay<T>>::CallExistsImpl(std::forward<T>(x));
}

template <typename OUTPUT, typename INPUT, bool HAS_VALUE_IMPL_METHOD>
struct PowerfulValueImplCaller {
  template <typename TT>
  static decltype(std::declval<TT>().template VariantValueImpl<OUTPUT>()) AccessValue(TT&& x) {
    return x.template VariantValueImpl<OUTPUT>();
  }
};

// For `OUTPUT == INPUT`, it's either plain `return x;`, or `return x.ValueImpl()`.
template <typename T>
struct PowerfulValueImplCaller<T, T, false> {
  using DECAYED_T = current::decay<T>;
  static const DECAYED_T& AccessValue(const DECAYED_T& x) { return x; }
  static DECAYED_T& AccessValue(DECAYED_T& x) { return x; }
  static DECAYED_T&& AccessValue(DECAYED_T&& x) { return std::move(x); }
};

template <typename T>
struct PowerfulValueImplCaller<T, T&, false> {
  using DECAYED_T = current::decay<T>;
  static DECAYED_T& AccessValue(DECAYED_T& x) { return x; }
};

template <typename T>
struct PowerfulValueImplCaller<T, const T&, false> {
  using DECAYED_T = current::decay<T>;
  static const DECAYED_T& AccessValue(const DECAYED_T& x) { return x; }
};

template <typename T>
struct PowerfulValueImplCaller<T, T, true> {
  template <typename TT>
  static decltype(std::declval<TT>().ValueImpl()) AccessValue(TT&& x) {
    return x.ValueImpl();
  }
};

struct DefaultValueInvocation {};
template <typename OUTPUT = DefaultValueInvocation, typename INPUT>
auto Value(INPUT&& x) -> decltype(PowerfulValueImplCaller<
    typename std::conditional<std::is_same<OUTPUT, DefaultValueInvocation>::value, INPUT, OUTPUT>::type,
    INPUT,
    sfinae::ValueImplMethodTest<INPUT>::value>::AccessValue(std::declval<INPUT>())) {
  return PowerfulValueImplCaller<
      typename std::conditional<std::is_same<OUTPUT, DefaultValueInvocation>::value, INPUT, OUTPUT>::type,
      INPUT,
      sfinae::ValueImplMethodTest<INPUT>::value>::AccessValue(std::forward<INPUT>(x));
}

// MSVS is not friendly with `std::enable_if_t` as return type, but OK with it as a template parameter. -- D.K.
template <typename T>
struct CheckIntegrityImplCaller {
  template <typename TT = T, class = std::enable_if_t<!sfinae::HasCheckIntegrityImplMethod<TT>(0)>>
  static void CallCheckIntegrityImpl(TT&&) {}

  template <typename TT = T, class = std::enable_if_t<sfinae::HasCheckIntegrityImplMethod<TT>(0)>>
  static std::enable_if_t<sfinae::HasCheckIntegrityImplMethod<TT>(0)> CallCheckIntegrityImpl(TT&& x) {
    x.CheckIntegrityImpl();
  }
};

template <typename T>
void CheckIntegrity(T&& x) {
  CheckIntegrityImplCaller<T>::CallCheckIntegrityImpl(std::forward<T>(x));
}

}  // namespace current

using current::Exists;
using current::Value;
using current::CheckIntegrity;

#endif  // CURRENT_TYPE_SYSTEM_HELPERS_H
