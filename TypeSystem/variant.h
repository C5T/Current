/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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

// NOTE: Each type witin a `Variant` must be a `CURRENT_STRUCT`. No nesting is allowed.

#ifndef CURRENT_TYPE_SYSTEM_VARIANT_H
#define CURRENT_TYPE_SYSTEM_VARIANT_H

#include "../port.h"  // `make_unique`.

#include <memory>

#ifdef FEWER_COMPILE_TIME_CHECKS
// For runtime, not compile-time, extra checks.
#include <iostream>
#include <typeindex>
#include <unordered_map>
#endif

#include "base.h"
#include "helpers.h"
#include "exceptions.h"

#include "../Bricks/template/combine.h"
#include "../Bricks/template/mapreduce.h"
#include "../Bricks/template/typelist.h"
#include "../Bricks/template/rtti_dynamic_call.h"

namespace current {

struct BypassVariantTypeCheck {};

namespace variant {

#ifdef FEWER_COMPILE_TIME_CHECKS
template <typename... TS>
struct PopulateForAllTypes;
template <>
struct PopulateForAllTypes<> {
  static void DoIt(std::unordered_map<std::type_index, const char*>&) {}
};
template <typename T, typename... TS>
struct PopulateForAllTypes<T, TS...> {
  static void DoIt(std::unordered_map<std::type_index, const char*>& result) {
    const std::type_index ti = typeid(T);
    CURRENT_ASSERT(!result.count(ti));
    result[ti] = reflection::CurrentTypeNameAsConstCharPtr<T>();
    PopulateForAllTypes<TS...>::DoIt(result);
  }
};
template <typename... TS>
struct RuntimeTypeListHelpersImpl {
  using map_t = std::unordered_map<std::type_index, const char*>;
  const map_t types_;
  static map_t PopulateTypes() {
    map_t result;
    PopulateForAllTypes<TS...>::DoIt(result);
    return result;
  }
  RuntimeTypeListHelpersImpl() : types_(PopulateTypes()) {}
  template <typename T>
  void AssertContains() const {
    if (types_.find(std::type_index(typeid(T))) == types_.end()) {
      std::cerr << "Variant type mismatch.\n\tType: " << reflection::CurrentTypeNameAsConstCharPtr<T>()
                << "\nIs not among:";
      for (const auto& type : types_) {
        std::cerr << ' ' << type.second;
      }
      std::cerr << '\n';
      CURRENT_THROW(IncompatibleVariantTypeException<T>());
    }
  }
};
template <typename T>
struct RuntimeTypeListHelpers;
template <typename... TS>
struct RuntimeTypeListHelpers<TypeListImpl<TS...>> {
  template <typename U>
  static void AssertContains() {
    Singleton<RuntimeTypeListHelpersImpl<TS...>>().template AssertContains<U>();
  }
};
#endif  // FEWER_COMPILE_TIME_CHECKS

}  // namespace current::variant

struct IHasUncheckedMoveFromUniquePtr {
  virtual void UncheckedMoveFromUniquePtr(std::unique_ptr<CurrentStruct>) = 0;
};

// Note: `Variant<...>` never uses `TypeList<...>`, only `TypeListImpl<...>`.
// Thus, it emphasizes performance over correctness.
// The user hold the risk of having duplicate types, and it's their responsibility to pass in a `TypeList<...>`
// instead of a `TypeListImpl<...>` in such a case, to ensure type de-duplication takes place.

// Initializes an `std::unique_ptr<CurrentStruct>` given the object of the right type.
// The input object could be an object itself (in which case it's copied),
// an `std::move()`-d `std::unique_ptr` to that object (in which case it's moved),
// or a bare pointer (in which case it's captured).
template <typename NAME, typename TYPE_LIST>
struct VariantImpl;

template <typename NAME, typename... TYPES>
struct VariantImpl<NAME, TypeListImpl<TYPES...>> : CurrentVariantImpl<NAME>, IHasUncheckedMoveFromUniquePtr {
  using typelist_t = TypeListImpl<TYPES...>;

  static constexpr size_t typelist_size = typelist_t::size;

  template <typename OTHER_NAME, typename OTHER_TYPE_LIST>
  friend struct VariantImpl;

  VariantImpl() {}

  VariantImpl(BypassVariantTypeCheck, std::unique_ptr<CurrentStruct>&& rhs) : object_(std::move(rhs)) {}

  // Use deep copy helper for all Variant types, including our own.
  VariantImpl(const VariantImpl& rhs) { CopyFrom(rhs); }
  template <typename... RHS>
  VariantImpl(const VariantImpl<RHS...>& rhs) {
    CopyFrom(rhs);
  }

  // Default move constructor for the same Variant type as ours.
  VariantImpl(VariantImpl&& rhs) = default;

  // Other Variants are processed using a type-aware move helper.
  template <typename... RHS>
  VariantImpl(VariantImpl<RHS...>&& rhs) {
    MoveFrom(std::forward<VariantImpl<RHS...>>(rhs));
  }

#ifdef FEWER_COMPILE_TIME_CHECKS
  template <typename X, class ENABLE_IF = std::enable_if_t<IS_CURRENT_STRUCT(current::decay<X>)>>
  VariantImpl(X&& input) {
    using decayed_x = current::decay<X>;
    variant::RuntimeTypeListHelpers<typelist_t>::template AssertContains<decayed_x>();
    object_ = std::make_unique<decayed_x>(std::forward<X>(input));
  }
#else
  template <typename X, class ENABLE = std::enable_if_t<TypeListContains<typelist_t, current::decay<X>>::value>>
  VariantImpl(X&& input) {
    using decayed_t = current::decay<X>;
    object_ = std::make_unique<decayed_t>(std::forward<X>(input));
  }
#endif  // FEWER_COMPILE_TIME_CHECKS

  void operator=(std::nullptr_t) { object_ = nullptr; }

  VariantImpl& operator=(const VariantImpl& rhs) {
    CopyFrom(rhs);
    return *this;
  }

  VariantImpl& operator=(VariantImpl&& rhs) {
    object_ = std::move(rhs.object_);
    return *this;
  }

#ifdef FEWER_COMPILE_TIME_CHECKS
  template <typename X, class ENABLE = std::enable_if_t<IS_CURRENT_STRUCT(current::decay<X>)>>
#else
  template <typename X, class ENABLE = std::enable_if_t<TypeListContains<typelist_t, current::decay<X>>::value>>
#endif  // FEWER_COMPILE_TIME_CHECKS
  VariantImpl& operator=(X&& input) {
    using decayed_t = current::decay<X>;
    object_ = std::make_unique<decayed_t>(std::forward<X>(input));
    return *this;
  }

  void UncheckedMoveFromUniquePtr(std::unique_ptr<CurrentStruct> input) override { object_ = std::move(input); }

  operator bool() const { return object_ ? true : false; }

  template <typename F>
  void Call(F&& f) {
    if (object_) {
      current::metaprogramming::RTTIDynamicCall<typelist_t>(*object_, std::forward<F>(f));
    } else {
      CURRENT_THROW(UninitializedVariantOfTypeException<TYPES...>());
    }
  }

  template <typename F>
  void Call(F&& f) const {
    if (object_) {
      current::metaprogramming::RTTIDynamicCall<typelist_t>(*object_, std::forward<F>(f));
    } else {
      CURRENT_THROW(UninitializedVariantOfTypeException<TYPES...>());
    }
  }

  // By design, `VariantExistsImpl<T>()` and `VariantValueImpl<T>()` do not check
  // whether `X` is part of `typelist_t`. More specifically, they pass if `dynamic_cast<>` succeeds,
  // and thus will successfully retrieve a derived type as a base one,
  // regardless of whether the base one is present in `typelist_t`.
  // Use `Call()` to run a strict check.

  bool ExistsImpl() const { return (object_.get() != nullptr); }

  template <typename X>
  std::enable_if_t<!std::is_same<X, CurrentStruct>::value, bool> VariantExistsImpl() const {
    return dynamic_cast<const X*>(object_.get()) != nullptr;
  }

  // Variant `Exists<>` when queried for its own top-level `Variant` type.
  template <typename X>
  std::enable_if_t<std::is_same<X, VariantImpl>::value, bool> VariantExistsImpl() const {
    return ExistsImpl();
  }

  template <typename X>
  std::enable_if_t<!std::is_same<X, CurrentStruct>::value, X&> VariantValueImpl() {
    X* ptr = dynamic_cast<X*>(object_.get());
    if (ptr) {
      return *ptr;
    } else {
      CURRENT_THROW(NoValueOfTypeException<X>());
    }
  }

  template <typename X>
  const X& VariantValueImpl() const {
    const X* ptr = dynamic_cast<const X*>(object_.get());
    if (ptr) {
      return *ptr;
    } else {
      CURRENT_THROW(NoValueOfTypeException<X>());
    }
  }

  // Variant returns itself when `Value<>`-queried for its own top-level `Variant` type.
  template <typename X>
  std::enable_if_t<std::is_same<X, VariantImpl>::value, VariantImpl&> VariantValueImpl() {
    if (ExistsImpl()) {
      return *this;
    } else {
      CURRENT_THROW(NoValueOfTypeException<VariantImpl>());
    }
  }

  template <typename X>
  std::enable_if_t<std::is_same<X, VariantImpl>::value, const VariantImpl&> VariantValueImpl() const {
    if (ExistsImpl()) {
      return *this;
    } else {
      CURRENT_THROW(NoValueOfTypeException<VariantImpl>());
    }
  }

 private:
  struct TypeAwareClone {
    std::unique_ptr<CurrentStruct>& into;
    TypeAwareClone(std::unique_ptr<CurrentStruct>& into) : into(into) {}

#ifdef FEWER_COMPILE_TIME_CHECKS
    template <typename U>
    void operator()(const U& instance) {
      using decayed_u = current::decay<U>;
      variant::RuntimeTypeListHelpers<typelist_t>::template AssertContains<decayed_u>();
      into = std::make_unique<decayed_u>(instance);
    }
#else
    template <typename U>
    std::enable_if_t<TypeListContains<typelist_t, current::decay<U>>::value> operator()(const U& instance) {
      into = std::make_unique<current::decay<U>>(instance);
    }

    template <typename U>
    std::enable_if_t<!TypeListContains<typelist_t, current::decay<U>>::value> operator()(const U&) {
      CURRENT_THROW(IncompatibleVariantTypeException<current::decay<U>>());
    }
#endif  // FEWER_COMPILE_TIME_CHECKS
  };

  struct TypeAwareMove {
    // `from` should not be an rvalue reference, as the move operation in `operator()` may still throw.
    std::unique_ptr<CurrentStruct>& from;
    std::unique_ptr<CurrentStruct>& into;
    TypeAwareMove(std::unique_ptr<CurrentStruct>& from, std::unique_ptr<CurrentStruct>& into)
        : from(from), into(into) {}

#ifdef FEWER_COMPILE_TIME_CHECKS
    template <typename U>
    void operator()(U&&) {
      using decayed_u = current::decay<U>;
      variant::RuntimeTypeListHelpers<typelist_t>::template AssertContains<decayed_u>();
      into = std::move(from);
    }
#else
    template <typename U>
    std::enable_if_t<TypeListContains<typelist_t, current::decay<U>>::value> operator()(U&&) {
      into = std::move(from);
    }

    template <typename U>
    std::enable_if_t<!TypeListContains<typelist_t, current::decay<U>>::value> operator()(U&&) {
      CURRENT_THROW(IncompatibleVariantTypeException<current::decay<U>>());
    }
#endif  // FEWER_COMPILE_TIME_CHECKS
  };

  template <typename... RHS>
  void CopyFrom(const VariantImpl<RHS...>& rhs) {
    if (rhs.object_) {
      TypeAwareClone cloner(object_);
      rhs.Call(cloner);
    } else {
      object_ = nullptr;
    }
  }

  template <typename... RHS>
  void MoveFrom(VariantImpl<RHS...>&& rhs) {
    if (rhs.object_) {
      TypeAwareMove mover(rhs.object_, object_);
      rhs.Call(mover);
    } else {
      object_ = nullptr;
    }
  }

 private:
  std::unique_ptr<CurrentStruct> object_;
};

// `Variant<...>` can accept either a list of types, or a `TypeList<...>`.
template <template <typename> class NAME, typename T, typename... TS>
struct VariantSelector {
  using typelist_t = TypeListImpl<T, TS...>;
  using type = VariantImpl<NAME<typelist_t>, typelist_t>;
};

template <template <typename> class NAME, typename T, typename... TS>
struct VariantSelector<NAME, TypeListImpl<T, TS...>> {
  using typelist_t = TypeListImpl<T, TS...>;
  using type = VariantImpl<NAME<typelist_t>, typelist_t>;
};

template <typename... TS>
using Variant = typename VariantSelector<reflection::CurrentVariantDefaultName, TS...>::type;

// `NamedVariant` is only used from the `CURRENT_VARIANT` macro, so only support `TS...`, not `TypeList<TS...>`.
template <class NAME, typename... TS>
using NamedVariant = VariantImpl<NAME, TypeListImpl<TS...>>;

}  // namespace current

using current::Variant;

#define CURRENT_VARIANT(name, ...)                         \
  template <int>                                           \
  struct CURRENT_VARIANT_MACRO;                            \
  template <>                                              \
  struct CURRENT_VARIANT_MACRO<__COUNTER__> {              \
    static const char* VariantNameImpl() { return #name; } \
  };                                                       \
  using name = ::current::NamedVariant<CURRENT_VARIANT_MACRO<__COUNTER__ - 1>, __VA_ARGS__>;

#endif  // CURRENT_TYPE_SYSTEM_VARIANT_H
