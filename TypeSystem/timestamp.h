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

// `MicroTimestampOf(x)` returns the timestamp of `x`.
// It supports `CURRENT_STRUCT`-s with `CURRENT_USE_FIELD_AS_TIMESTAMP()`
//
// `Variant<...>` objects (recursively), raw `Epoch{Micro/Milli}seconds, and custom implementations
// of the `template<typename F> ReportTimestamp(F&& f) { f(timestamp); }` method.
//
// Purpose:
// 1) To timestamp stream entries and ensure they form a strictly increasing order.
// 2) To output the timestamp as a top-level field when subscribing to the stream via JSON.

#ifndef TYPE_SYSTEM_TIMESTAMP_H
#define TYPE_SYSTEM_TIMESTAMP_H

#include "../port.h"

#include "variant.h"

#include "../Bricks/time/chrono.h"
#include "../Bricks/template/is_unique_ptr.h"

namespace current {

template <typename T>
struct TimestampAccessorImpl;

struct ExtractTimestampFunctor {
  std::chrono::microseconds& result;
  ExtractTimestampFunctor(std::chrono::microseconds& result) : result(result) {}
  template <typename T>
  void operator()(T&& x) {
    TimestampAccessorImpl<current::decay<T>>::ExtractDirectlyOrFromVariant(*this, std::forward<T>(x));
  }
};

struct UpdateTimestampFunctor {
  std::chrono::microseconds value;
  UpdateTimestampFunctor(std::chrono::microseconds value) : value(value) {}
  template <typename T>
  void operator()(T&& x) {
    TimestampAccessorImpl<current::decay<T>>::UpdateDirectlyOrInVariant(*this, std::forward<T>(x));
  }
};

template <typename T>
struct TimestampAccessorImpl {
  template <typename E>
  static void ExtractDirectlyOrFromVariant(ExtractTimestampFunctor& functor, E&& e) {
    e.ReportTimestamp(functor);
  }
  template <typename E>
  static void UpdateDirectlyOrInVariant(UpdateTimestampFunctor& functor, E& e) {
    e.ReportTimestamp(functor);
  }
};

template <typename... TS>
struct TimestampAccessorImpl<VariantImpl<TS...>> {
  static void ExtractDirectlyOrFromVariant(ExtractTimestampFunctor& functor, const VariantImpl<TS...>& p) {
    p.Call(functor);
  }
  static void UpdateDirectlyOrInVariant(UpdateTimestampFunctor& functor, VariantImpl<TS...>& p) {
    p.Call(functor);
  }
};

template <>
struct TimestampAccessorImpl<std::chrono::microseconds> {
  static void ExtractDirectlyOrFromVariant(ExtractTimestampFunctor& functor,
                                           const std::chrono::microseconds& us) {
    functor.result = us;
  }
  static void UpdateDirectlyOrInVariant(UpdateTimestampFunctor& functor, std::chrono::microseconds& us) {
    us = functor.value;
  }
};

template <>
struct TimestampAccessorImpl<int64_t> {
  static void ExtractDirectlyOrFromVariant(ExtractTimestampFunctor& functor, int64_t us) {
    functor.result = std::chrono::microseconds(us);
  }
  static void UpdateDirectlyOrInVariant(UpdateTimestampFunctor& functor, int64_t& us) {
    us = functor.value.count();
  }
};

// TODO(dkorolev): The `IS_UNIQUE_PTR` helper is going away as we move off Cereal to our Variant<>.
template <bool IS_UNIQUE_PTR>
struct ExtractTimestampFromUniquePtrAsWell {};

template <>
struct ExtractTimestampFromUniquePtrAsWell<false> {
  template <typename E>
  static std::chrono::microseconds DoIt(E&& e) {
    std::chrono::microseconds result;
    ExtractTimestampFunctor impl(result);
    e.ReportTimestamp(impl);
    return result;
  }
};

template <>
struct ExtractTimestampFromUniquePtrAsWell<true> {
  template <typename E>
  static std::chrono::microseconds DoIt(E&& e) {
    std::chrono::microseconds result;
    ExtractTimestampFunctor impl(result);
    e->ReportTimestamp(impl);
    return result;
  }
};

template <typename E>
std::chrono::microseconds MicroTimestampOf(E&& entry) {
  return ExtractTimestampFromUniquePtrAsWell<current::is_unique_ptr<E>::value>::template DoIt<E>(
      std::forward<E>(entry));
}

// TODO(dkorolev): Test this function too, not just use it from `examples/EmbeddedDB`.
template <typename E>
void SetMicroTimestamp(E& entry, std::chrono::microseconds timestamp) {
  UpdateTimestampFunctor impl(timestamp);
  entry.ReportTimestamp(impl);
}

}  // namespace current

#endif  // TYPE_SYSTEM_TIMESTAMP_H
