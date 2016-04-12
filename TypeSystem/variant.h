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

#ifndef CURRENT_TYPE_SYSTEM_VARIANT_H
#define CURRENT_TYPE_SYSTEM_VARIANT_H

#include "../port.h"  // `make_unique`.

#include <memory>

#include "base.h"
#include "helpers.h"
#include "exceptions.h"

#include "../Bricks/template/combine.h"
#include "../Bricks/template/mapreduce.h"
#include "../Bricks/template/typelist.h"
#include "../Bricks/template/rtti_dynamic_call.h"

namespace current {

// A high-octane version of `current::metaprogramming::combine.
namespace variant_high_octane_tmp {

using current::metaprogramming::EvensOnly;

template <typename T>
struct fast_combine;

template <typename T, typename... TS>
struct fast_combine<TypeListImpl<T, TS...>> : fast_combine<EvensOnly<TypeListImpl<T, TS...>>>,
                                              fast_combine<EvensOnly<TypeListImpl<TS...>>> {
  using fast_combine<EvensOnly<TypeListImpl<T, TS...>>>::operator();
  using fast_combine<EvensOnly<TypeListImpl<TS...>>>::operator();
};

template <typename T>
struct fast_combine<TypeListImpl<T>> {
  T instance;

  template <typename PARAM>
  void operator()(typename T::first_parameter_1 first, PARAM&& param, typename T::third_parameter& third) {
    instance.operator()(first, std::forward<PARAM>(param), third);
  }

  template <typename PARAM>
  void operator()(typename T::first_parameter_2 first, PARAM&& param, typename T::third_parameter& third) {
    instance.operator()(first, std::forward<PARAM>(param), third);
  }
};

}  // namespace variant_high_octane_tmp

// Note: `Variant<...>` never uses `TypeList<...>`, only `TypeListImpl<...>`.
// Thus, it emphasizes performance over correctness.
// The user hold the risk of having duplicate types, and it's their responsibility to pass in a `TypeList<...>`
// instead of a `TypeListImpl<...>` in such a case, to ensure type de-duplication takes place.

// Initializes an `std::unique_ptr<CurrentSuper>` given the object of the right type.
// The input object could be an object itself (in which case it's copied),
// an `std::move()`-d `std::unique_ptr` to that object (in which case it's moved),
// or a bare pointer (in which case it's captured).
// TODO(dkorolev): The bare pointer one is sure unsafe -- consult with @mzhurovich.
template <typename TYPELIST>
struct VariantTypeCheckedAssignment;

template <typename... TS>
struct VariantTypeCheckedAssignment<TypeListImpl<TS...>> {
  template <typename Z>
  struct DerivedTypesDifferentiator {};

  template <typename X>
  struct Impl {
    // Legal parameter types, for high-octane metaprogramming.
    using first_parameter_1 = DerivedTypesDifferentiator<X>;
    using first_parameter_2 = DerivedTypesDifferentiator<std::unique_ptr<X>>;
    using third_parameter = std::unique_ptr<CurrentSuper>;

    // Copy `X`.
    void operator()(DerivedTypesDifferentiator<X>,
                    const X& source,
                    std::unique_ptr<CurrentSuper>& destination) {
      if (!destination || !dynamic_cast<X*>(destination.get())) {
        destination = std::make_unique<X>();
      }
      // Note: `destination.get() = source` is a mistake, and we made sure it doesn't compile. -- D.K.
      dynamic_cast<X&>(*destination.get()) = source;
    }
    // Move `X`.
    void operator()(DerivedTypesDifferentiator<X>, X&& source, std::unique_ptr<CurrentSuper>& destination) {
      if (!destination || !dynamic_cast<X*>(destination.get())) {
        destination = std::make_unique<X>();
      }
      // Note: `destination.get() = source` is a mistake, and we made sure it doesn't compile. -- D.K.
      dynamic_cast<X&>(*destination.get()) = std::move(source);
    }
    // Move `std::unique_ptr`.
    void operator()(DerivedTypesDifferentiator<std::unique_ptr<X>>,
                    std::unique_ptr<X>&& source,
                    std::unique_ptr<CurrentSuper>& destination) {
      if (!source) {
        throw NoValueOfTypeException<X>();  // LCOV_EXCL_LINE
      }
      destination = std::move(source);
    }
  };

  using Instance = variant_high_octane_tmp::fast_combine<TypeListImpl<Impl<TS>...>>;

  template <typename Q>
  static void Perform(Q&& source, std::unique_ptr<CurrentSuper>& destination) {
    Instance instance;
    instance(DerivedTypesDifferentiator<current::decay<Q>>(), std::forward<Q>(source), destination);
  }
};

template <typename... TYPES>
struct VariantImpl;

template <typename... TYPES>
struct VariantImpl<TypeListImpl<TYPES...>> : CurrentVariant {
  using typelist_t = TypeListImpl<TYPES...>;
  using DEPRECATED_T_(TYPELIST) = typelist_t;
  enum { typelist_size = TypeListSize<typelist_t>::value };

  std::unique_ptr<CurrentSuper> object_;

  VariantImpl() {}

  operator bool() const { return object_ ? true : false; }

  VariantImpl(std::unique_ptr<CurrentSuper>&& rhs) : object_(std::move(rhs)) {}

  VariantImpl(const VariantImpl<typelist_t>& rhs) { CopyFrom(rhs); }

  VariantImpl(VariantImpl<typelist_t>&& rhs) : object_(std::move(rhs.object_)) {}

  VariantImpl& operator=(const VariantImpl<typelist_t>& rhs) {
    CopyFrom(rhs);
    return *this;
  }

  VariantImpl& operator=(VariantImpl<typelist_t>&& rhs) {
    object_ = std::move(rhs.object_);
    return *this;
  }

  template <typename X,
            bool ENABLE = !std::is_same<current::decay<X>, VariantImpl<TypeListImpl<TYPES...>>>::value,
            class SFINAE = ENABLE_IF<ENABLE>>
  void operator=(X&& input) {
    VariantTypeCheckedAssignment<typelist_t>::Perform(std::forward<X>(input), object_);
  }

  template <typename... TS>
  void operator=(VariantImpl<TS...>&& rhs) {
    TypeAwareMove mover(*this);
    rhs.Call(mover);
  }

  template <typename X,
            bool ENABLE = !std::is_same<current::decay<X>, VariantImpl<TypeListImpl<TYPES...>>>::value,
            class SFINAE = ENABLE_IF<ENABLE>>
  VariantImpl(X&& input) {
    VariantTypeCheckedAssignment<typelist_t>::Perform(std::forward<X>(input), object_);
    CheckIntegrityImpl();
  }

  template <typename... TS>
  VariantImpl(VariantImpl<TS...>&& rhs) {
    TypeAwareMove mover(*this);
    rhs.Call(mover);
  }

  void operator=(std::nullptr_t) { object_ = nullptr; }

  template <typename F>
  void Call(F&& f) {
    CheckIntegrityImpl();
    current::metaprogramming::RTTIDynamicCall<typelist_t>(*object_, std::forward<F>(f));
  }

  template <typename F>
  void Call(F&& f) const {
    CheckIntegrityImpl();
    current::metaprogramming::RTTIDynamicCall<typelist_t>(*object_, std::forward<F>(f));
  }

  // By design, `VariantExistsImpl<T>()` and `VariantValueImpl<T>()` do not check
  // whether `X` is part of `typelist_t`. More specifically, they pass if `dynamic_cast<>` succeeds,
  // and thus will successfully retrieve a derived type as a base one,
  // regardless of whether the base one is present in `typelist_t`.
  // Use `Call()` to run a strict check.

  bool ExistsImpl() const { return (object_.get() != nullptr); }

  template <typename X>
  ENABLE_IF<!std::is_same<X, CurrentStruct>::value, bool> VariantExistsImpl() const {
    return dynamic_cast<const X*>(object_.get()) != nullptr;
  }

  // Variant `Exists<>` when queried for its own top-level `Variant` type.
  template <typename X>
  ENABLE_IF<std::is_same<X, VariantImpl>::value, bool> VariantExistsImpl() const {
    return ExistsImpl();
  }

  template <typename X>
  ENABLE_IF<!std::is_same<X, CurrentStruct>::value, X&> VariantValueImpl() {
    X* ptr = dynamic_cast<X*>(object_.get());
    if (ptr) {
      return *ptr;
    } else {
      throw NoValueOfTypeException<X>();
    }
  }

  template <typename X>
  ENABLE_IF<!std::is_same<X, CurrentSuper>::value, const X&> VariantValueImpl() const {
    const X* ptr = dynamic_cast<const X*>(object_.get());
    if (ptr) {
      return *ptr;
    } else {
      throw NoValueOfTypeException<X>();
    }
  }

  // Variant returns itself when `Value<>`-queried for its own top-level `Variant` type.
  template <typename X>
  ENABLE_IF<std::is_same<X, VariantImpl>::value, VariantImpl&> VariantValueImpl() {
    if (ExistsImpl()) {
      return *this;
    } else {
      throw NoValueOfTypeException<VariantImpl>();
    }
  }

  template <typename X>
  ENABLE_IF<std::is_same<X, VariantImpl>::value, const VariantImpl&> VariantValueImpl() const {
    if (ExistsImpl()) {
      return *this;
    } else {
      throw NoValueOfTypeException<VariantImpl>();
    }
  }

  void CheckIntegrityImpl() const {
    if (!object_) {
      throw UninitializedVariantOfTypeException<TYPES...>();  // LCOV_EXCL_LINE
    }
  }

 private:
  struct TypeAwareClone {
    VariantImpl<TypeListImpl<TYPES...>>& result;
    TypeAwareClone(VariantImpl<TypeListImpl<TYPES...>>& result) : result(result) {}

    template <typename TT>
    void operator()(const TT& instance) {
      result = instance;
    }
  };

  struct TypeAwareMove {
    VariantImpl<TypeListImpl<TYPES...>>& result;
    TypeAwareMove(VariantImpl<TypeListImpl<TYPES...>>& result) : result(result) {}

    template <typename TT>
    void operator()(TT&& instance) {
      result = std::move(instance);
    }
  };

  void CopyFrom(const VariantImpl<TypeListImpl<TYPES...>>& rhs) {
    if (rhs.object_) {
      TypeAwareClone cloner(*this);
      rhs.Call(cloner);
      CheckIntegrityImpl();
    } else {
      object_ = nullptr;
    }
  }
};

// `Variant<...>` can accept either a list of types, or a `TypeList<...>`.
template <typename T, typename... TS>
struct VariantSelector {
  using type = VariantImpl<TypeListImpl<T, TS...>>;
};

template <typename T, typename... TS>
struct VariantSelector<TypeListImpl<T, TS...>> {
  using type = VariantImpl<TypeListImpl<T, TS...>>;
};

template <typename... TS>
using Variant = typename VariantSelector<TS...>::type;

}  // namespace current

using current::Variant;

#endif  // CURRENT_TYPE_SYSTEM_VARIANT_H
