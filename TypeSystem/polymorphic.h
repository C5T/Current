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

#ifndef CURRENT_TYPE_SYSTEM_POLYMORPHIC_H
#define CURRENT_TYPE_SYSTEM_POLYMORPHIC_H

#include "../port.h"  // `make_unique`.

#include <memory>

#include "base.h"
#include "exceptions.h"

#include "../Bricks/template/combine.h"
#include "../Bricks/template/mapreduce.h"
#include "../Bricks/template/typelist.h"
#include "../Bricks/template/rtti_dynamic_call.h"

namespace current {

// Note: `Polymorphic<...>` never uses `TypeList<...>`, only `TypeListImpl<...>`.
// Thus, it emphasizes performance over correctness.
// The user hold the risk of having duplicate types, and it's their responsibility to pass in a `TypeList<...>`
// instead of a `TypeListImpl<...>` in such a case, to ensure type de-duplication takes place.

// Explicitly disable an empty `Polymorphic<>`.
template <typename... TS>
struct PolymorphicImpl;

template <typename T, typename... TS>
struct PolymorphicImpl<T, TS...> {
  using T_TYPELIST = TypeListImpl<T, TS...>;
  enum { T_TYPELIST_SIZE = TypeListSize<T_TYPELIST>::value };

  std::unique_ptr<CurrentSuper> object_;

  // Initializes an `std::unique_ptr<CurrentSuper>` given the object of the right type.
  // The input object could be an object itself (in which case it's copied),
  // an `std::move()`-d `std::unique_ptr` to that object (in which cast it's moved),
  // or a bare pointer (in which case it's captured).
  // TODO(dkorolev): The bare pointer one is sure unsafe -- consult with @mzhurovich.
  struct TypeCheckedAsisgnment {
    template <typename X>
    struct Impl {
      // Copy `X`.
      void operator()(const X& source, std::unique_ptr<CurrentSuper>& destination) {
        if (!destination || !dynamic_cast<X*>(destination.get())) {
          destination = make_unique<X>();
        }
        X& placeholder = dynamic_cast<X&>(*destination.get());
        placeholder = source;
      }
      // Move `X`.
      void operator()(X&& source, std::unique_ptr<CurrentSuper>& destination) {
        if (!destination || !dynamic_cast<X*>(destination.get())) {
          destination = make_unique<X>();
        }
        X& placeholder = dynamic_cast<X&>(*destination.get());
        placeholder = std::move(source);
      }
      // Move `std::unique_ptr`.
      void operator()(std::unique_ptr<X>&& source, std::unique_ptr<CurrentSuper>& destination) {
        if (!source) {
          throw NoValueOfTypeException<X>();
        }
        destination = std::move(source);
      }
      // Capture a bare `X*`.
      // TODO(dkorolev): Unsafe? Remove?
      void operator()(X* source, std::unique_ptr<CurrentSuper>& destination) { destination.reset(source); }
    };
    using Instance = bricks::metaprogramming::combine<bricks::metaprogramming::map<Impl, T_TYPELIST>>;
    template <typename Q>
    static void Perform(Q&& source, std::unique_ptr<CurrentSuper>& destination) {
      Instance instance;
      instance(std::forward<Q>(source), destination);
    }
  };

  PolymorphicImpl() = delete;

  // PolymorphicImpl(std::unique_ptr<CurrentSuper>&& rhs) : object_(std::move(rhs)) {}
  template <typename X>
  PolymorphicImpl(X&& input) {
    TypeCheckedAsisgnment::Perform(std::forward<X>(input), object_);
  }

  PolymorphicImpl(PolymorphicImpl&& rhs) : object_(std::move(rhs.object)) {}

  template <typename X>
  void operator=(X&& input) {
    TypeCheckedAsisgnment::Perform(std::forward<X>(input), object_);
  }

  template<typename F>
  void Match(F&& f) {
    bricks::metaprogramming::RTTIDynamicCall<T_TYPELIST>(*object_, std::forward<F>(f));
  }

  template<typename F>
  void Match(F&& f) const {
    bricks::metaprogramming::RTTIDynamicCall<T_TYPELIST>(*object_, std::forward<F>(f));
  }

  // Note that `Has()` and `Value()` do not check whether `X` is part of `T_TYPELIST`.
  // This is a) for performance reasons, and b) to allow accessing base types from derived ones.
  // Use `Match()` to run a strict check.
  template <typename X>
  bool Has() const {
    return dynamic_cast<const X*>(object_.get()) != nullptr;
  }

  template <typename X>
  X& Value() {
    X* ptr = dynamic_cast<X*>(object_.get());
    if (ptr) {
      return *ptr;
    } else {
      throw NoValueOfTypeException<X>();
    }
  }

  template <typename X>
  const X& Value() const {
    const X* ptr = dynamic_cast<const X*>(object_.get());
    if (ptr) {
      return *ptr;
    } else {
      throw NoValueOfTypeException<X>();
    }
  }
};

// `Polymorphic<...>` can accept either a list of types, or a `TypeList<...>`.
template <typename... TS>
struct PolymorphicSelector {
  using type = PolymorphicImpl<TS...>;
};

template <typename... TS>
struct PolymorphicSelector<TypeListImpl<TS...>> {
  using type = PolymorphicImpl<TS...>;
};

template <typename... TS>
using Polymorphic = typename PolymorphicSelector<TS...>::type;

}  // namespace current

using current::Polymorphic;

#endif  // CURRENT_TYPE_SYSTEM_POLYMORPHIC_H
