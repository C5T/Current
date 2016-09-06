/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

// RTTI-based call dispatching to the handler of the appropriate type.

#ifndef BRICKS_RTTI_DISPATCHER_H
#define BRICKS_RTTI_DISPATCHER_H

#include "exceptions.h"

#include "../template/typelist.h"

namespace current {
namespace rtti {

template <typename BASE, typename DERIVED, typename... TAIL>
struct RuntimeDispatcher {
  typedef BASE base_t;
  typedef DERIVED deriver_t;
  template <typename T, typename PROCESSOR>
  static void DispatchCall(const T &x, PROCESSOR &c) {
    if (const DERIVED *d = dynamic_cast<const DERIVED *>(&x)) {
      c(*d);
    } else {
      RuntimeDispatcher<BASE, TAIL...>::DispatchCall(x, c);
    }
  }
  template <typename T, typename PROCESSOR>
  static void DispatchCall(T &x, PROCESSOR &c) {
    if (DERIVED *d = dynamic_cast<DERIVED *>(&x)) {
      c(*d);
    } else {
      RuntimeDispatcher<BASE, TAIL...>::DispatchCall(x, c);
    }
  }
};

template <typename BASE, typename DERIVED>
struct RuntimeDispatcher<BASE, DERIVED> {
  typedef BASE base_t;
  typedef DERIVED deriver_t;
  template <typename T, typename PROCESSOR>
  static void DispatchCall(const T &x, PROCESSOR &c) {
    if (const DERIVED *d = dynamic_cast<const DERIVED *>(&x)) {
      c(*d);
    } else {
      const BASE *b = dynamic_cast<const BASE *>(&x);
      if (b) {
        c(*b);
      } else {
        CURRENT_THROW(UnrecognizedPolymorphicType());
      }
    }
  }
  template <typename T, typename PROCESSOR>
  static void DispatchCall(T &x, PROCESSOR &c) {
    if (DERIVED *d = dynamic_cast<DERIVED *>(&x)) {
      c(*d);
    } else {
      BASE *b = dynamic_cast<BASE *>(&x);
      if (b) {
        c(*b);
      } else {
        CURRENT_THROW(UnrecognizedPolymorphicType());
      }
    }
  }
};

template <typename BASE, class TYPE_LIST>
struct RuntimeTypeListDispatcher;

template <typename BASE, typename... TYPES>
struct RuntimeTypeListDispatcher<BASE, TypeListImpl<TYPES...>> : RuntimeDispatcher<BASE, TYPES...> {};

}  // namespace current::rtti
}  // namespace current

#endif  // BRICKS_RTTI_DISPATCHER_H
