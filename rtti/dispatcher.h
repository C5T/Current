#ifndef BRICKS_RTTI_DISPATCHER_H
#define BRICKS_RTTI_DISPATCHER_H

#include "exceptions.h"

#include <tuple>

namespace bricks {
namespace rtti {

template <typename BASE, typename DERIVED, typename... TAIL>
struct RuntimeDispatcher {
  typedef BASE T_BASE;
  typedef DERIVED T_DERIVED;
  template <typename TYPE, typename PROCESSOR>
  static void DispatchCall(const TYPE &x, PROCESSOR &c) {
    if (const DERIVED *d = dynamic_cast<const DERIVED *>(&x)) {
      c(*d);
    } else {
      RuntimeDispatcher<BASE, TAIL...>::DispatchCall(x, c);
    }
  }
  template <typename TYPE, typename PROCESSOR>
  static void DispatchCall(TYPE &x, PROCESSOR &c) {
    if (DERIVED *d = dynamic_cast<DERIVED *>(&x)) {
      c(*d);
    } else {
      RuntimeDispatcher<BASE, TAIL...>::DispatchCall(x, c);
    }
  }
};

template <typename BASE, typename DERIVED>
struct RuntimeDispatcher<BASE, DERIVED> {
  typedef BASE T_BASE;
  typedef DERIVED T_DERIVED;
  template <typename TYPE, typename PROCESSOR>
  static void DispatchCall(const TYPE &x, PROCESSOR &c) {
    if (const DERIVED *d = dynamic_cast<const DERIVED *>(&x)) {
      c(*d);
    } else {
      const BASE *b = dynamic_cast<const BASE *>(&x);
      if (b) {
        c(*b);
      } else {
        throw UnrecognizedPolymorphicType();
      }
    }
  }
  template <typename TYPE, typename PROCESSOR>
  static void DispatchCall(TYPE &x, PROCESSOR &c) {
    if (DERIVED *d = dynamic_cast<DERIVED *>(&x)) {
      c(*d);
    } else {
      BASE *b = dynamic_cast<BASE *>(&x);
      if (b) {
        c(*b);
      } else {
        throw UnrecognizedPolymorphicType();
      }
    }
  }
};

template <typename BASE, typename... TUPLE_TYPES>
struct RuntimeTupleDispatcher {};

template <typename BASE, typename... TUPLE_TYPES>
struct RuntimeTupleDispatcher<BASE, std::tuple<TUPLE_TYPES...>> : RuntimeDispatcher<BASE, TUPLE_TYPES...> {};

}  // namespace rtti
}  // namespace bricks

#endif  // BRICKS_RTTI_DISPATCHER_H
