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
#include "helpers.h"
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

struct DisableDefaultConstructorForRequired {};

template <bool ENABLED>
struct DefaultConstructorGuard {
  DefaultConstructorGuard() = default;
  DefaultConstructorGuard(const DisableDefaultConstructorForRequired&) = delete;
};

template <>
struct DefaultConstructorGuard<true> {
  DefaultConstructorGuard() = default;
  DefaultConstructorGuard(const DisableDefaultConstructorForRequired&) {}
};

// Initializes an `std::unique_ptr<CurrentSuper>` given the object of the right type.
// The input object could be an object itself (in which case it's copied),
// an `std::move()`-d `std::unique_ptr` to that object (in which case it's moved),
// or a bare pointer (in which case it's captured).
// TODO(dkorolev): The bare pointer one is sure unsafe -- consult with @mzhurovich.
template <class TYPELIST>
struct PolymorphicTypeCheckedAssignment {
  template <typename Z>
  struct DerivedTypesDifferentiator {};

  template <typename X>
  struct Impl {
    // Copy `X`.
    void operator()(DerivedTypesDifferentiator<X>,
                    const X& source,
                    std::unique_ptr<CurrentSuper>& destination) {
      if (!destination || !dynamic_cast<X*>(destination.get())) {
        destination = MakeUnique<X>();
      }
      // Note: `destination.get() = source` is a mistake, and we made sure it doesn't compile. -- D.K.
      dynamic_cast<X&>(*destination.get()) = Clone(source);
    }
    // Move `X`.
    void operator()(DerivedTypesDifferentiator<X>, X&& source, std::unique_ptr<CurrentSuper>& destination) {
      if (!destination || !dynamic_cast<X*>(destination.get())) {
        destination = MakeUnique<X>();
      }
      // Note: `destination.get() = source` is a mistake, and we made sure it doesn't compile. -- D.K.
      dynamic_cast<X&>(*destination.get()) = std::move(source);
    }
    // Move `std::unique_ptr`.
    void operator()(DerivedTypesDifferentiator<std::unique_ptr<X>>,
                    std::unique_ptr<X>&& source,
                    std::unique_ptr<CurrentSuper>& destination) {
      if (!source) {
        throw NoValueOfTypeException<X>();
      }
      destination = std::move(source);
    }
    // Capture a bare `X*`.
    // TODO(dkorolev): Unsafe? Remove?
    void operator()(DerivedTypesDifferentiator<X*>, X* source, std::unique_ptr<CurrentSuper>& destination) {
      destination.reset(source);
    }
  };
  using Instance = current::metaprogramming::combine<current::metaprogramming::map<Impl, TYPELIST>>;
  template <typename Q>
  static void Perform(Q&& source, std::unique_ptr<CurrentSuper>& destination) {
    Instance instance;
    instance(DerivedTypesDifferentiator<current::decay<Q>>(), std::forward<Q>(source), destination);
  }
};

template <bool REQUIRED, typename... TYPES>
struct GenericPolymorphicImpl<false, REQUIRED, TypeListImpl<TYPES...>, TypeListImpl<TYPES...>>
    : DefaultConstructorGuard<!REQUIRED> {
  using T_TYPELIST = TypeListImpl<TYPES...>;
  enum { T_TYPELIST_SIZE = TypeListSize<T_TYPELIST>::value };
  enum { IS_REQUIRED = REQUIRED, IS_OPTIONAL = !REQUIRED };

  using STRIPPED_TYPE =
      GenericPolymorphicImpl<true, REQUIRED, metaprogramming::map<Stripped, T_TYPELIST>, T_TYPELIST>;

  using UNSTRIPPED_TYPE = GenericPolymorphicImpl<false, REQUIRED, T_TYPELIST, T_TYPELIST>;

  std::unique_ptr<CurrentSuper> object_;

  GenericPolymorphicImpl() : DefaultConstructorGuard<!REQUIRED>(DisableDefaultConstructorForRequired()) {}
  GenericPolymorphicImpl(ForceDefaultConstructionDespiteDeletedConstructor) {}

  template <bool ENABLE = !REQUIRED, class = ENABLE_IF<ENABLE>>
  operator bool() const {
    return object_ ? true : false;
  }

  GenericPolymorphicImpl(std::unique_ptr<CurrentSuper>&& rhs) : object_(std::move(rhs)) {}

  template <bool U, bool R, class TL>
  GenericPolymorphicImpl(GenericPolymorphicImpl<U, R, TL, T_TYPELIST>&& rhs)
      : object_(std::move(rhs.object_)) {
    CheckIntegrityImpl();
  }

  template <bool U, bool R, class TL>
  GenericPolymorphicImpl& operator=(GenericPolymorphicImpl<U, R, TL, T_TYPELIST>&& rhs) {
    object_ = std::move(rhs.object_);
    CheckIntegrityImpl();
    return *this;
  }

  template <typename X,
            bool ENABLE = !IS_POLYMORPHIC<current::decay<X>>::value,
            class SFINAE = ENABLE_IF<ENABLE>>
  void operator=(X&& input) {
    PolymorphicTypeCheckedAssignment<T_TYPELIST>::Perform(std::forward<X>(input), object_);
  }

  template <typename X,
            bool ENABLE = !IS_POLYMORPHIC<current::decay<X>>::value,
            class SFINAE = ENABLE_IF<ENABLE>>
  GenericPolymorphicImpl(X&& input) {
    PolymorphicTypeCheckedAssignment<T_TYPELIST>::Perform(std::forward<X>(input), object_);
    CheckIntegrityImpl();
  }

  template <bool ENABLE = !REQUIRED>
  ENABLE_IF<ENABLE> operator=(std::nullptr_t) {
    object_ = nullptr;
  }

  template <typename F>
  void Call(F&& f) {
    CheckIntegrityImpl();
    current::metaprogramming::RTTIDynamicCall<T_TYPELIST>(*object_, std::forward<F>(f));
  }

  template <typename F>
  void Call(F&& f) const {
    CheckIntegrityImpl();
    current::metaprogramming::RTTIDynamicCall<T_TYPELIST>(*object_, std::forward<F>(f));
  }

  // By design, `PolymorphicExistsImpl<T>()` and `PolymorphicValueImpl<T>()` do not check
  // whether `X` is part of `T_TYPELIST`. More specifically, they pass if `dynamic_cast<>` succeeds,
  // and thus will successfully retrieve a derived type as a base one,
  // regardless of whether the base one is present in `T_TYPELIST`.
  // Use `Call()` to run a strict check.

  template <bool ENABLE = !REQUIRED>
  ENABLE_IF<ENABLE, bool> ExistsImpl() const {
    return (object_.get() != nullptr);
  }

  template <typename X>
  ENABLE_IF<!std::is_same<X, CurrentSuper>::value, bool> PolymorphicExistsImpl() const {
    return dynamic_cast<const X*>(object_.get()) != nullptr;
  }

  template <typename X>
  ENABLE_IF<!std::is_same<X, CurrentSuper>::value, X&> PolymorphicValueImpl() {
    X* ptr = dynamic_cast<X*>(object_.get());
    if (ptr) {
      return *ptr;
    } else {
      throw NoValueOfTypeException<X>();
    }
  }

  template <typename X>
  ENABLE_IF<!std::is_same<X, CurrentSuper>::value, const X&> PolymorphicValueImpl() const {
    const X* ptr = dynamic_cast<const X*>(object_.get());
    if (ptr) {
      return *ptr;
    } else {
      throw NoValueOfTypeException<X>();
    }
  }

  void CheckIntegrityImpl() const {
    if (REQUIRED && !object_) {
      throw UninitializedRequiredPolymorphicOfTypeException<TYPES...>();
    }
  }
};

// This specialiation completely resolves the problem of non-redeclarable constructors. -- D.K.
template <bool REQUIRED, typename... TYPES, typename... ORIGINAL_TYPES>
struct GenericPolymorphicImpl<true, REQUIRED, TypeListImpl<TYPES...>, TypeListImpl<ORIGINAL_TYPES...>> {
  using T_JOINED_TYPELIST = TypeListImpl<TYPES..., ORIGINAL_TYPES...>;

  using STRIPPED_TYPE =
      GenericPolymorphicImpl<true, REQUIRED, TypeListImpl<TYPES...>, TypeListImpl<ORIGINAL_TYPES...>>;
  using UNSTRIPPED_TYPE =
      GenericPolymorphicImpl<false, REQUIRED, TypeListImpl<ORIGINAL_TYPES...>, TypeListImpl<ORIGINAL_TYPES...>>;

  std::unique_ptr<CurrentSuper> object_;

  GenericPolymorphicImpl() = default;

  operator bool() const { return object_ ? true : false; }

  GenericPolymorphicImpl(std::unique_ptr<CurrentSuper>&& rhs) : object_(std::move(rhs)) {}

  template <bool U, bool R, class TL1, class TL2>
  GenericPolymorphicImpl(const GenericPolymorphicImpl<U, R, TL1, TL2>& rhs)
      : object_(std::move(rhs.object_)) {}

  template <bool U, bool R, class TL1, class TL2>
  GenericPolymorphicImpl(GenericPolymorphicImpl<U, R, TL1, TL2>&& rhs)
      : object_(std::move(rhs.object_)) {}

  template <bool U, bool R, class TL1, class TL2>
  GenericPolymorphicImpl& operator=(GenericPolymorphicImpl<U, R, TL1, TL2>&& rhs) {
    object_ = std::move(rhs.object_);
    return *this;
  }

  template <typename X,
            bool ENABLE = !IS_POLYMORPHIC<current::decay<X>>::value,
            class SFINAE = ENABLE_IF<ENABLE>>
  GenericPolymorphicImpl(X&& input) {
    PolymorphicTypeCheckedAssignment<T_JOINED_TYPELIST>::Perform(std::forward<X>(input), object_);
  }

  template <typename X,
            bool ENABLE = !IS_POLYMORPHIC<current::decay<X>>::value,
            class SFINAE = ENABLE_IF<ENABLE>>
  void operator=(X&& input) {
    PolymorphicTypeCheckedAssignment<T_JOINED_TYPELIST>::Perform(std::forward<X>(input), object_);
  }

  GenericPolymorphicImpl& operator=(std::nullptr_t) {
    object_ = nullptr;
    return *this;
  }

  struct TypeAwareClone {
    GenericPolymorphicImpl<true, REQUIRED, TypeListImpl<TYPES...>, TypeListImpl<ORIGINAL_TYPES...>>& result;
    TypeAwareClone(
        GenericPolymorphicImpl<true, REQUIRED, TypeListImpl<TYPES...>, TypeListImpl<ORIGINAL_TYPES...>>& result)
        : result(result) {}
    template <typename TT>
    void operator()(const TT& instance) {
      result = Clone(instance);
    }
  };

  // Copy constructor and assignment operator are legal for stripped versions.
  GenericPolymorphicImpl(const GenericPolymorphicImpl& rhs) {
    TypeAwareClone cloner(*this);
    current::metaprogramming::RTTIDynamicCall<T_JOINED_TYPELIST>(*rhs.object_, cloner);
  }

  GenericPolymorphicImpl& operator=(const GenericPolymorphicImpl& rhs) {
    TypeAwareClone cloner(*this);
    current::metaprogramming::RTTIDynamicCall<T_JOINED_TYPELIST>(*rhs.object_, cloner);
    return *this;
  }

  // By design, `PolymorphicExistsImpl<T>()` and `PolymorphicValueImpl<T>()` do not check
  // whether `X` is part of `T_TYPELIST`. More specifically, they pass if `dynamic_cast<>` succeeds,
  // and thus will successfully retrieve a derived type as a base one,
  // regardless of whether the base one is present in `T_TYPELIST`.
  // Use `Call()` to run a strict check.
  template <bool ENABLE = !REQUIRED>
  ENABLE_IF<ENABLE, bool> ExistsImpl() const {
    return (object_.get() != nullptr);
  }

  template <typename X>
  ENABLE_IF<!std::is_same<X, CurrentSuper>::value, bool> PolymorphicExistsImpl() const {
    return dynamic_cast<const X*>(object_.get()) != nullptr;
  }

  template <typename X>
  ENABLE_IF<!std::is_same<X, CurrentSuper>::value, X&> PolymorphicValueImpl() {
    X* ptr = dynamic_cast<X*>(object_.get());
    if (ptr) {
      return *ptr;
    } else {
      throw NoValueOfTypeException<X>();
    }
  }

  template <typename X>
  ENABLE_IF<!std::is_same<X, CurrentSuper>::value, const X&> PolymorphicValueImpl() const {
    const X* ptr = dynamic_cast<const X*>(object_.get());
    if (ptr) {
      return *ptr;
    } else {
      throw NoValueOfTypeException<X>();
    }
  }
};

// Explicitly disable an empty `Polymorphic<>`.

// `Polymorphic<...>` can accept either a list of types, or a `TypeList<...>`.
template <typename T, typename... TS>
struct PolymorphicSelector {
  using type = GenericPolymorphicImpl<false, true, TypeListImpl<T, TS...>, TypeListImpl<T, TS...>>;
};

template <typename T, typename... TS>
struct PolymorphicSelector<TypeListImpl<T, TS...>> {
  using type = GenericPolymorphicImpl<false, true, TypeListImpl<T, TS...>, TypeListImpl<T, TS...>>;
};

template <typename... TS>
using Polymorphic = typename PolymorphicSelector<TS...>::type;

// `OptionalPolymorphic<...>` can accept either a list of types, or a `TypeList<...>`.
template <typename T, typename... TS>
struct OptionalPolymorphicSelector {
  using type = GenericPolymorphicImpl<false, false, TypeListImpl<T, TS...>, TypeListImpl<T, TS...>>;
};

template <typename T, typename... TS>
struct OptionalPolymorphicSelector<TypeListImpl<T, TS...>> {
  using type = GenericPolymorphicImpl<false, false, TypeListImpl<T, TS...>, TypeListImpl<T, TS...>>;
};

template <typename... TS>
using OptionalPolymorphic = typename OptionalPolymorphicSelector<TS...>::type;

}  // namespace current

using current::Polymorphic;
using current::OptionalPolymorphic;

#endif  // CURRENT_TYPE_SYSTEM_POLYMORPHIC_H
