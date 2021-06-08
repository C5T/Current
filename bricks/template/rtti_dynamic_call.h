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

#include "../../port.h"

#include <string>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

#include "decay.h"
#include "typelist.h"
#include "tuple.h"

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

enum class DispatcherInputType { ConstReference, Reference, RValueReference };

template <DispatcherInputType, typename BASE, typename F, typename... ARGS>
struct RTTIDispatcherBase;

template <typename BASE, typename F, typename... ARGS>
struct RTTIDispatcherBase<DispatcherInputType::ConstReference, BASE, F, ARGS...> {
  virtual ~RTTIDispatcherBase() = default;
  virtual void HandleByConstReference(const BASE&, F&&, ARGS&&...) const {
    CURRENT_THROW(SpecificUnhandledTypeException<BASE>());
  }
};

template <typename BASE, typename F, typename... ARGS>
struct RTTIDispatcherBase<DispatcherInputType::Reference, BASE, F, ARGS...> {
  virtual ~RTTIDispatcherBase() = default;
  virtual void HandleByReference(BASE&, F&&, ARGS&&...) const { CURRENT_THROW(SpecificUnhandledTypeException<BASE>()); }
};

template <typename BASE, typename F, typename... ARGS>
struct RTTIDispatcherBase<DispatcherInputType::RValueReference, BASE, F, ARGS...> {
  virtual ~RTTIDispatcherBase() = default;
  virtual void HandleByRValueReference(BASE&&, F&&, ARGS&&...) const {
    CURRENT_THROW(SpecificUnhandledTypeException<BASE>());
  }
};
// LCOV_EXCL_STOP

template <DispatcherInputType, typename BASE, typename F, typename DERIVED, typename... ARGS>
struct RTTIDispatcher;

template <typename BASE, typename F, typename DERIVED, typename... ARGS>
struct RTTIDispatcher<DispatcherInputType::ConstReference, BASE, F, DERIVED, ARGS...> final
    : RTTIDispatcherBase<DispatcherInputType::ConstReference, BASE, F, ARGS...> {
  virtual void HandleByConstReference(const BASE& ref, F&& f, ARGS&&... args) const override {
    const auto* derived = dynamic_cast<const DERIVED*>(&ref);
    if (derived) {
      f(*derived, std::forward<ARGS>(args)...);
    } else {
      CURRENT_THROW((SpecificUncastableTypeException<BASE, DERIVED>()));  // LCOV_EXCL_LINE
    }
  }
};

template <typename BASE, typename F, typename DERIVED, typename... ARGS>
struct RTTIDispatcher<DispatcherInputType::Reference, BASE, F, DERIVED, ARGS...> final
    : RTTIDispatcherBase<DispatcherInputType::Reference, BASE, F, ARGS...> {
  virtual void HandleByReference(BASE& ref, F&& f, ARGS&&... args) const override {
    auto* derived = dynamic_cast<DERIVED*>(&ref);
    if (derived) {
      f(*derived, std::forward<ARGS>(args)...);
    } else {
      CURRENT_THROW((SpecificUncastableTypeException<BASE, DERIVED>()));  // LCOV_EXCL_LINE
    }
  }
};

template <typename BASE, typename F, typename DERIVED, typename... ARGS>
struct RTTIDispatcher<DispatcherInputType::RValueReference, BASE, F, DERIVED, ARGS...> final
    : RTTIDispatcherBase<DispatcherInputType::RValueReference, BASE, F, ARGS...> {
  virtual void HandleByRValueReference(BASE&& ref, F&& f, ARGS&&... args) const override {
    auto* derived = dynamic_cast<DERIVED*>(&ref);
    if (derived) {
      f(std::move(*derived), std::forward<ARGS>(args)...);
    } else {
      CURRENT_THROW((SpecificUncastableTypeException<BASE, DERIVED>()));  // LCOV_EXCL_LINE
    }
  }
};

template <DispatcherInputType DISPATCHER_INPUT_TYPE, typename BASE, typename F, typename... ARGS>
using RTTIHandlersMap =
    std::unordered_map<std::type_index, std::unique_ptr<RTTIDispatcherBase<DISPATCHER_INPUT_TYPE, BASE, F, ARGS...>>>;

template <DispatcherInputType, typename TYPELIST, typename BASE, typename F, typename... ARGS>
struct PopulateRTTIHandlers;

template <DispatcherInputType DISPATCHER_INPUT_TYPE, typename BASE, typename F, typename... ARGS>
struct PopulateRTTIHandlers<DISPATCHER_INPUT_TYPE, std::tuple<>, BASE, F, ARGS...> {
  static void DoIt(RTTIHandlersMap<DISPATCHER_INPUT_TYPE, BASE, F, ARGS...>&) {}
};

template <DispatcherInputType DISPATCHER_INPUT_TYPE,
          typename T,
          typename... TS,
          typename BASE,
          typename F,
          typename... ARGS>
struct PopulateRTTIHandlers<DISPATCHER_INPUT_TYPE, std::tuple<T, TS...>, BASE, F, ARGS...> {
  static void DoIt(RTTIHandlersMap<DISPATCHER_INPUT_TYPE, BASE, F, ARGS...>& map) {
    // TODO(dkorolev): Check for duplicate types in input type list? Throw an exception?
    map[std::type_index(typeid(T))].reset(new RTTIDispatcher<DISPATCHER_INPUT_TYPE, BASE, F, T, ARGS...>());
    PopulateRTTIHandlers<DISPATCHER_INPUT_TYPE, std::tuple<TS...>, BASE, F, ARGS...>::DoIt(map);
  }
};

// Support both `std::tuple<>` and `TypeList<>` for now. -- D.K.
// TODO(dkorolev): Revisit this once we will be retiring `std::tuple<>`'s use as typelist.
template <DispatcherInputType DISPATCHER_INPUT_TYPE, typename BASE, typename F, typename... ARGS>
struct PopulateRTTIHandlers<DISPATCHER_INPUT_TYPE, TypeListImpl<>, BASE, F, ARGS...> {
  static void DoIt(RTTIHandlersMap<DISPATCHER_INPUT_TYPE, BASE, F, ARGS...>&) {}
};

template <DispatcherInputType DISPATCHER_INPUT_TYPE,
          typename T,
          typename... TS,
          typename BASE,
          typename F,
          typename... ARGS>
struct PopulateRTTIHandlers<DISPATCHER_INPUT_TYPE, TypeListImpl<T, TS...>, BASE, F, ARGS...> {
  static void DoIt(RTTIHandlersMap<DISPATCHER_INPUT_TYPE, BASE, F, ARGS...>& map) {
    // TODO(dkorolev): Check for duplicate types in input type list? Throw an exception?
    map[std::type_index(typeid(T))].reset(new RTTIDispatcher<DISPATCHER_INPUT_TYPE, BASE, F, T, ARGS...>());
    PopulateRTTIHandlers<DISPATCHER_INPUT_TYPE, TypeListImpl<TS...>, BASE, F, ARGS...>::DoIt(map);
  }
};

template <DispatcherInputType DISPATCHER_INPUT_TYPE, typename TYPELIST, typename BASE, typename F, typename... ARGS>
struct RTTIPopulatedHandlers {
  static_assert(is_std_tuple<TYPELIST>::value || IsTypeList<TYPELIST>::value, "");
  RTTIHandlersMap<DISPATCHER_INPUT_TYPE, BASE, F, ARGS...> map;
  RTTIPopulatedHandlers() { PopulateRTTIHandlers<DISPATCHER_INPUT_TYPE, TYPELIST, BASE, F, ARGS...>::DoIt(map); }
};

template <DispatcherInputType DISPATCHER_INPUT_TYPE, typename TYPELIST, typename BASE, typename F, typename... ARGS>
const RTTIDispatcherBase<DISPATCHER_INPUT_TYPE, BASE, F, ARGS...>* RTTIFindHandler(const std::type_info& type) {
  static_assert(is_std_tuple<TYPELIST>::value || IsTypeList<TYPELIST>::value, "");
  static RTTIPopulatedHandlers<DISPATCHER_INPUT_TYPE, TYPELIST, BASE, F, ARGS...> singleton;
  const auto handler = singleton.map.find(std::type_index(type));
  if (handler != singleton.map.end()) {
    return handler->second.get();
  } else {
    CURRENT_THROW(SpecificUnlistedTypeException<BASE>());  // LCOV_EXCL_LINE
  }
}

// Returning values from RTTI-dispatched calls is not supported. Pass in another parameter by pointer/reference.
template <typename TYPELIST, typename BASE>
struct RTTIDynamicCallWrapper {
  template <typename... REST>
  static void RunHandle(const BASE& ref, REST&&... rest) {
    RTTIFindHandler<DispatcherInputType::ConstReference, TYPELIST, BASE, REST...>(typeid(ref))
        ->HandleByConstReference(ref, std::forward<REST>(rest)...);
  }
  template <typename... REST>
  static void RunHandle(BASE& ref, REST&&... rest) {
    RTTIFindHandler<DispatcherInputType::Reference, TYPELIST, BASE, REST...>(typeid(ref))
        ->HandleByReference(ref, std::forward<REST>(rest)...);
  }
  template <typename... REST>
  static void RunHandle(BASE&& ref, REST&&... rest) {
    RTTIFindHandler<DispatcherInputType::RValueReference, TYPELIST, BASE, REST...>(typeid(ref))
        ->HandleByRValueReference(std::move(ref), std::forward<REST>(rest)...);
  }
};

template <typename TYPELIST, typename BASE, typename... REST>
void RTTIDynamicCall(BASE&& ref, REST&&... rest) {
  RTTIDynamicCallWrapper<TYPELIST, current::decay_t<BASE>>::RunHandle(std::forward<BASE>(ref),
                                                                      std::forward<REST>(rest)...);
}

}  // namespace metaprogramming
}  // namespace current

using current::metaprogramming::RTTIDynamicCall;

#endif  // BRICKS_TEMPLATE_RTTI_DYNAMIC_CALL_H
