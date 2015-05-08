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

#ifndef BRICKS_TEMPLATE_RTTI_DYNAMIC_CALL_H
#define BRICKS_TEMPLATE_RTTI_DYNAMIC_CALL_H

#include <string>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

#include "is_tuple.h"

#include "../exception.h"
#include "../util/singleton.h"

namespace bricks {
namespace metaprogramming {

struct RTTIException : Exception {
  RTTIException() = delete;
  explicit RTTIException(const std::string& s) : Exception(s) {}
};

struct UnlistedTypeException : RTTIException {
  UnlistedTypeException() = delete;
  explicit UnlistedTypeException(const std::string& s) : RTTIException(s) {}
};

template <typename T>
struct SpecificUnlistedTypeException : UnlistedTypeException {
  explicit SpecificUnlistedTypeException() : UnlistedTypeException(typeid(T).name()) {}
};

struct UnhandledTypeException : RTTIException {
  UnhandledTypeException() = delete;
  explicit UnhandledTypeException(const std::string& s) : RTTIException(s) {}
};

template <typename T>
struct SpecificUnhandledTypeException : UnhandledTypeException {
  explicit SpecificUnhandledTypeException() : UnhandledTypeException(typeid(T).name()) {}
};

struct UncastableTypeException : RTTIException {
  UncastableTypeException() = delete;
  explicit UncastableTypeException(const std::string& s) : RTTIException(s) {}
};

template <typename BASE, typename DERIVED>
struct SpecificUncastableTypeException : UncastableTypeException {
  explicit SpecificUncastableTypeException()
      : UncastableTypeException(typeid(BASE).name() + std::string(" -> ") + typeid(DERIVED).name()) {}
};

template <typename BASE, typename F>
struct RTTIDispatcherBase {
  virtual void Handle(BASE&, F&&) const { throw SpecificUnhandledTypeException<BASE>(); }
};

template <typename BASE, typename F, typename DERIVED>
struct RTTIDispatcher : RTTIDispatcherBase<BASE, F> {
  virtual void Handle(BASE& ptr, F&& f) const override {
    DERIVED* derived = dynamic_cast<DERIVED*>(&ptr);
    if (derived) {
      f(*derived);
    } else {
      throw SpecificUncastableTypeException<BASE, DERIVED>();
    }
  }
};

template <typename BASE, typename F>
using RTTIHandlersMap = std::unordered_map<std::type_index, std::unique_ptr<RTTIDispatcherBase<BASE, F>>>;

template <typename TYPELIST, typename BASE, typename F>
struct PopulateRTTIHandlers {};

template <typename BASE, typename F>
struct PopulateRTTIHandlers<std::tuple<>, BASE, F> {
  static void DoIt(RTTIHandlersMap<BASE, F>&) {}
};

template <typename T, typename... TS, typename BASE, typename F>
struct PopulateRTTIHandlers<std::tuple<T, TS...>, BASE, F> {
  static void DoIt(RTTIHandlersMap<BASE, F>& map) {
    // TODO(dkorolev): Check for duplicate types in input type list? Throw an exception?
    map[std::type_index(typeid(T))].reset(new RTTIDispatcher<BASE, F, T>());
    PopulateRTTIHandlers<std::tuple<TS...>, BASE, F>::DoIt(map);
  }
};

template <typename TYPELIST, typename BASE, typename F>
struct RTTIPopulatedHandlers {
  static_assert(is_std_tuple<TYPELIST>::value, "");
  RTTIHandlersMap<BASE, F> map;
  RTTIPopulatedHandlers() { PopulateRTTIHandlers<TYPELIST, BASE, F>::DoIt(map); }
};

template <typename TYPELIST, typename BASE, typename F>
const RTTIDispatcherBase<BASE, F>* RTTIFindHandler(const std::type_info& type) {
  static_assert(is_std_tuple<TYPELIST>::value, "");
  const RTTIHandlersMap<BASE, F>& map = ThreadLocalSingleton<RTTIPopulatedHandlers<TYPELIST, BASE, F>>().map;
  const auto handler = map.find(std::type_index(type));
  if (handler != map.end()) {
    return handler->second.get();
  } else {
    throw SpecificUnlistedTypeException<BASE>();
  }
}

// TODO(dkorolev): Should we support return value from these calls?
// TODO(dkorolev): Non-const `PTR` OK?
// TODO(dkorolev): Should we require `F` to derive from a special class
//                 that wraps all the types into pure virtual functions?
//                 This way forgetting either of them would be a compile error.
template <typename TYPELIST, typename BASE, typename F>
void RTTIDynamicCall(BASE& ptr, F&& f) {
  RTTIFindHandler<TYPELIST, BASE, F>(typeid(ptr))->Handle(ptr, std::forward<F>(f));
}

// A parital specialization for a `unique_ptr`.
template <typename TYPELIST, typename BASE, typename F>
void RTTIDynamicCall(std::unique_ptr<BASE>& ptr, F&& f) {
  RTTIDynamicCall<TYPELIST>(*ptr.get(), std::forward<F>(f));
}

}  // namespace metaprogramming
}  // namespace bricks

#endif  // BRICKS_TEMPLATE_RTTI_DYNAMIC_CALL_H
