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
// To make a copy of `x`, the recommended synopsis is `Clone(x)`.
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
#include "../Bricks/template/decay.h"

namespace current {

template <typename TEST, typename T>
struct ExistsImplCaller {
  template <typename TT = T>
  static bool CallExistsImpl(TT&& x) {
    return x.template PolymorphicExistsImpl<TEST>();
  }
};

template <typename T>
struct ExistsImplCaller<T, T> {
  // Primitive types.
  template <typename TT = T>
  static ENABLE_IF<!current::sfinae::HasExistsImplMethod<TT>(0), bool> CallExistsImpl(TT&&) {
    return true;
  }

  // Special types.
  template <typename TT = T>
  static ENABLE_IF<current::sfinae::HasExistsImplMethod<TT>(0), bool> CallExistsImpl(TT&& x) {
    return x.ExistsImpl();
  }
};

struct DefaultExistsInvokation {};
template <typename TEST = DefaultExistsInvokation, typename T>
bool Exists(T&& x) {
  return ExistsImplCaller<
      typename std::conditional<std::is_same<TEST, DefaultExistsInvokation>::value, T, TEST>::type,
      T>::CallExistsImpl(std::forward<T>(x));
}

template <typename OUTPUT, typename INPUT>
struct PowerfulValueImplCaller {
  using DECAYED_INPUT = current::decay<INPUT>;
  static OUTPUT& AccessValue(DECAYED_INPUT& x) { return x.template PolymorphicValueImpl<OUTPUT>(); }
  static const OUTPUT& AccessValue(const DECAYED_INPUT& x) { return x.template PolymorphicValueImpl<OUTPUT>(); }
  static OUTPUT&& AccessValue(DECAYED_INPUT&& x) { return x.template PolymorphicValueImpl<OUTPUT>(); }
};

// For `OUTPUT == INPUT`, it's either plain `return x;`, or `return x.ValueImpl()`.
template <typename T>
struct PowerfulValueImplCaller<T, T> {
  template <typename TT = T>
  static ENABLE_IF<!current::sfinae::HasValueImplMethod<TT>(0), T&&> AccessValue(T&& x) {
    return x;
  }
  template <typename TT = T>
  static ENABLE_IF<current::sfinae::HasValueImplMethod<TT>(0), decltype(std::declval<TT>().ValueImpl())>
  AccessValue(TT&& x) {
    return x.ValueImpl();
  }
};

struct DefaultValueInvokation {};
template <typename OUTPUT = DefaultValueInvokation, typename INPUT>
auto Value(INPUT&& x) -> decltype(PowerfulValueImplCaller<
    typename std::conditional<std::is_same<OUTPUT, DefaultValueInvokation>::value, INPUT, OUTPUT>::type,
    INPUT>::AccessValue(std::declval<INPUT>())) {
  return PowerfulValueImplCaller<
      typename std::conditional<std::is_same<OUTPUT, DefaultValueInvokation>::value, INPUT, OUTPUT>::type,
      INPUT>::AccessValue(std::forward<INPUT>(x));
}

template <typename T>
struct CheckIntegrityImplCaller {
  template <typename TT = T>
  static ENABLE_IF<!current::sfinae::HasCheckIntegrityImplMethod<TT>(0)> CallCheckIntegrityImpl(TT&&) {}

  template <typename TT = T>
  static ENABLE_IF<current::sfinae::HasCheckIntegrityImplMethod<TT>(0)> CallCheckIntegrityImpl(TT&& x) {
    x.CheckIntegrityImpl();
  }
};

template <typename T>
void CheckIntegrity(T&& x) {
  CheckIntegrityImplCaller<T>::CallCheckIntegrityImpl(std::forward<T>(x));
}

// TODO(dkorolev): Migrate the older `Clone()` methods here, after cleaning them up.
template <typename T>
decay<T> Clone(T&& x) {
  using DECAYED_T = decay<T>;
  static_assert(sizeof(DECAYED_T) == sizeof(Stripped<DECAYED_T>), "");
  // This `Clone()` method operates on three assumptions:
  // 1) `Stripped<T>` exists.
  // 2) `Stripped<T>` has a copy constructor.
  // 3) `T` has a move constructor.
  const Stripped<DECAYED_T>& copyable_x = *reinterpret_cast<const Stripped<DECAYED_T>*>(&x);
  Stripped<DECAYED_T> copy(copyable_x);
  DECAYED_T& returnable_copy(*reinterpret_cast<DECAYED_T*>(&copy));
  return std::move(returnable_copy);
}

}  // namespace current

using current::Exists;
using current::Value;
using current::CheckIntegrity;
using current::Clone;

#endif  // CURRENT_TYPE_SYSTEM_HELPERS_H
