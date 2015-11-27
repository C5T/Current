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

// `MicroTimestampOf(x)` returns the timestamp of `x`, as monotonically increasing, microsecond since epoch.
// `MicroTimestampOf(x)` support `CURRENT_STRUCT`-s that have a `TIMESTAMP_FIELD(field)` declared within
// themselves, or `CURRENT_STRUCT`-s that implement the `ReportTimestamp(F f) { f(timestamp); }` method
// manually.
// As an impotant extension, `TIMESTAMP_FIELD(field)` may refer to a polymorphic field, in which case
// the timestamp of the respective instance of that polymorphic field will be extracted.
//
// Purpose:
// 1) To timestamp stream entries and ensure they form a strictly increasing order.
// 2) To output the timestamp as a top-level field when subscribing to the stream via JSON.

#ifndef TYPE_SYSTEM_TIMESTAMP_H
#define TYPE_SYSTEM_TIMESTAMP_H

#include "../port.h"

#include "polymorphic.h"

#include "../Bricks/time/chrono.h"
#include "../Bricks/template/is_unique_ptr.h"

namespace current {

template <typename T>
struct ExtractTimestampImpl;

struct ExtractTimestampFunctor {
  EpochMicroseconds& result;
  ExtractTimestampFunctor(EpochMicroseconds& result) : result(result) {}
  template <typename T>
  void operator()(T&& x) {
    ExtractTimestampImpl<current::decay<T>>::DirectlyOrFromPolymorphic(*this, std::forward<T>(x));
  }
};

template <typename T>
struct ExtractTimestampImpl {
  template <typename E>
  static void DirectlyOrFromPolymorphic(ExtractTimestampFunctor& functor, E&& e) {
    e.ReportTimestamp(functor);
  }
};

template <typename... TS>
struct ExtractTimestampImpl<PolymorphicImpl<TS...>> {
  static void DirectlyOrFromPolymorphic(ExtractTimestampFunctor& functor, const PolymorphicImpl<TS...>& p) {
    p.Call(functor);
  }
};

template <>
struct ExtractTimestampImpl<EpochMicroseconds> {
  static void DirectlyOrFromPolymorphic(ExtractTimestampFunctor& functor, const EpochMicroseconds& us) {
    functor.result = us;
  }
};

template <>
struct ExtractTimestampImpl<EpochMilliseconds> {
  static void DirectlyOrFromPolymorphic(ExtractTimestampFunctor& functor, const EpochMilliseconds& ms) {
    functor.result = EpochMicroseconds(ms.ms * 1000ull);
  }
};

template <>
struct ExtractTimestampImpl<uint64_t> {
  static void DirectlyOrFromPolymorphic(ExtractTimestampFunctor& functor, uint64_t us) {
    functor.result = EpochMicroseconds(us);
  }
};

// TODO(dkorolev): The `IS_UNIQUE_PTR` helper is going away as we move off Cereal to our Polymorphic<>.
template <bool IS_UNIQUE_PTR>
struct ExtractTimestampFromUniquePtrAsWell {};

template <>
struct ExtractTimestampFromUniquePtrAsWell<false> {
  template <typename E>
  static EpochMicroseconds DoIt(E&& e) {
    EpochMicroseconds result;
    ExtractTimestampFunctor impl(result);
    e.ReportTimestamp(impl);
    return result;
  }
};

template <>
struct ExtractTimestampFromUniquePtrAsWell<true> {
  template <typename E>
  static EpochMicroseconds DoIt(E&& e) {
    EpochMicroseconds result;
    ExtractTimestampFunctor impl(result);
    e->ReportTimestamp(impl);
    return result;
  }
};

template <typename E>
EpochMicroseconds MicroTimestampOf(E&& entry) {
  return ExtractTimestampFromUniquePtrAsWell<current::is_unique_ptr<E>::value>::template DoIt<E>(
      std::forward<E>(entry));
}

}  // namespace current

#endif  // TYPE_SYSTEM_TIMESTAMP_H
