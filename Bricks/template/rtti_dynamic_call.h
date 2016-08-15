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
#include "is_unique_ptr.h"
#include "typelist.h"

#include "../exception.h"
#include "../util/singleton.h"

namespace current {
namespace metaprogramming {

// LCOV_EXCL_START
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

template <typename BASE, typename F, typename... ARGS>
struct RTTIDispatcherBase {
  virtual ~RTTIDispatcherBase() = default;
  virtual void Handle(BASE&&, F&&, ARGS&&...) const { CURRENT_THROW(SpecificUnhandledTypeException<BASE>()); }
};
// LCOV_EXCL_STOP

template <typename BASE, typename F, typename DERIVED, typename... ARGS>
struct RTTIDispatcher : RTTIDispatcherBase<BASE, F, ARGS...> {
  virtual void Handle(BASE&& ref, F&& f, ARGS&&... args) const override {
    // `auto` to support both `const` and mutable.
    auto derived = dynamic_cast_preserving_const<DERIVED>(&ref);
    if (derived) {
      f(*derived, std::forward<ARGS>(args)...);
    } else {
      CURRENT_THROW((SpecificUncastableTypeException<BASE, DERIVED>()));  // LCOV_EXCL_LINE
    }
  }
};

template <typename BASE, typename F, typename... ARGS>
using RTTIHandlersMap =
    std::unordered_map<std::type_index, std::unique_ptr<RTTIDispatcherBase<BASE, F, ARGS...>>>;

template <typename TYPELIST, typename BASE, typename F, typename... ARGS>
struct PopulateRTTIHandlers {};

template <typename BASE, typename F, typename... ARGS>
struct PopulateRTTIHandlers<std::tuple<>, BASE, F, ARGS...> {
  static void DoIt(RTTIHandlersMap<BASE, F, ARGS...>&) {}
};

template <typename T, typename... TS, typename BASE, typename F, typename... ARGS>
struct PopulateRTTIHandlers<std::tuple<T, TS...>, BASE, F, ARGS...> {
  static void DoIt(RTTIHandlersMap<BASE, F, ARGS...>& map) {
    // TODO(dkorolev): Check for duplicate types in input type list? Throw an exception?
    map[std::type_index(typeid(T))].reset(new RTTIDispatcher<BASE, F, T, ARGS...>());
    PopulateRTTIHandlers<std::tuple<TS...>, BASE, F, ARGS...>::DoIt(map);
  }
};

// Support both `std::tuple<>` and `TypeList<>` for now. -- D.K.
// TODO(dkorolev): Revisit this once we will be retiring `std::tuple<>`'s use as typelist.
template <typename BASE, typename F, typename... ARGS>
struct PopulateRTTIHandlers<TypeListImpl<>, BASE, F, ARGS...> {
  static void DoIt(RTTIHandlersMap<BASE, F, ARGS...>&) {}
};

template <typename T, typename... TS, typename BASE, typename F, typename... ARGS>
struct PopulateRTTIHandlers<TypeListImpl<T, TS...>, BASE, F, ARGS...> {
  static void DoIt(RTTIHandlersMap<BASE, F, ARGS...>& map) {
    // TODO(dkorolev): Check for duplicate types in input type list? Throw an exception?
    map[std::type_index(typeid(T))].reset(new RTTIDispatcher<BASE, F, T, ARGS...>());
    PopulateRTTIHandlers<TypeListImpl<TS...>, BASE, F, ARGS...>::DoIt(map);
  }
};

template <typename TYPELIST, typename BASE, typename F, typename... ARGS>
struct RTTIPopulatedHandlers {
  static_assert(is_std_tuple<TYPELIST>::value || IsTypeList<TYPELIST>::value, "");
  RTTIHandlersMap<BASE, F, ARGS...> map;
  RTTIPopulatedHandlers() { PopulateRTTIHandlers<TYPELIST, BASE, F, ARGS...>::DoIt(map); }
};

template <typename TYPELIST, typename BASE, typename F, typename... ARGS>
const RTTIDispatcherBase<BASE, F, ARGS...>* RTTIFindHandler(const std::type_info& type) {
  static_assert(is_std_tuple<TYPELIST>::value || IsTypeList<TYPELIST>::value, "");
  static RTTIPopulatedHandlers<TYPELIST, BASE, F, ARGS...> singleton;
  const auto handler = singleton.map.find(std::type_index(type));
  if (handler != singleton.map.end()) {
    return handler->second.get();
  } else {
    CURRENT_THROW(SpecificUnlistedTypeException<BASE>());  // LCOV_EXCL_LINE
  }
}

// Returning values from RTTI-dispatched calls is not supported. Pass in another parameter by pointer/reference.
template <typename TYPELIST, typename BASE, typename... REST>
void RTTIDynamicCallImpl(BASE&& ref, REST&&... rest) {
  RTTIFindHandler<TYPELIST, BASE, REST...>(typeid(ref))
      ->Handle(std::forward<BASE>(ref), std::forward<REST>(rest)...);
}

template <typename TYPELIST, typename BASE, typename... REST>
void RTTIDynamicCall(BASE&& ref, REST&&... rest) {
  RTTIDynamicCallImpl<TYPELIST>(is_unique_ptr<BASE>::extract(std::forward<BASE>(ref)),
                                std::forward<REST>(rest)...);
}

}  // namespace metaprogramming
}  // namespace current

using current::metaprogramming::RTTIDynamicCall;

#endif  // BRICKS_TEMPLATE_RTTI_DYNAMIC_CALL_H
