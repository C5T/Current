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

#ifndef CURRENT_TYPE_SYSTEM_TYPES_H
#define CURRENT_TYPE_SYSTEM_TYPES_H

#include "../port.h"

#include <map>
#include <memory>
#include <type_traits>
#include <utility>

#include "base.h"

#include "../bricks/template/decay.h"
#include "../bricks/template/pod.h"
#include "../bricks/template/variadic_indexes.h"

namespace crnt {

// The superclass for all Current-defined types, to enable polymorphic serialization and deserialization.
struct CurrentSuper {
  // "Implementing" an empty destructor here, as `= default;` makes this destructor `noexcept`,
  // which causes problems with the Windows build, as some other destructors are not `noexcept`.
  // TODO(dkorolev): I'll fix this one day, but not today.
  virtual ~CurrentSuper() {}
};

// For `unique_ptr<>`-s.
struct CurrentSuperDeleter {
  void operator()(CurrentSuper* ptr) { delete ptr; }
};

// The superclass for all Current structures.
struct CurrentStruct : CurrentSuper {};

// The superclass for `Variant` type.
struct CurrentVariant : CurrentSuper {};

#define IS_CURRENT_STRUCT(T) (std::is_base_of_v<::crnt::CurrentStruct, ::current::decay_t<T>>)
#define IS_CURRENT_VARIANT(T) (std::is_base_of_v<::crnt::CurrentVariant, ::current::decay_t<T>>)
#define IS_CURRENT_STRUCT_OR_VARIANT(T) (std::is_base_of_v<::crnt::CurrentSuper, ::current::decay_t<T>>)

#define IS_EMPTY_CURRENT_STRUCT(T) (::current::reflection::IsEmptyCurrentStruct<T, IS_CURRENT_STRUCT(T)>::value)

namespace sfinae {

// Whether an `ExistsImpl()` method is defined for a type.
template <typename ENTRY>
constexpr bool HasExistsImplMethod(char) {
  return false;
}

template <typename ENTRY>
constexpr auto HasExistsImplMethod(int) -> decltype(std::declval<const ENTRY>().ExistsImpl(), bool()) {
  return true;
}

template <typename ENTRY>
struct ExistsImplMethodTest {
  static constexpr bool value = HasExistsImplMethod<ENTRY>(0);
};

// Whether a `ValueImpl()` method is defined for a type.
template <typename ENTRY>
constexpr bool HasValueImplMethod(char) {
  return false;
}

template <typename ENTRY>
constexpr auto HasValueImplMethod(int) -> decltype(std::declval<const ENTRY>().ValueImpl(), bool()) {
  return true;
}

template <typename ENTRY>
struct ValueImplMethodTest {
  static constexpr bool value = HasValueImplMethod<ENTRY>(0);
};

// Whether a `SuccessfulImpl()` method is defined for a type.
template <typename ENTRY>
constexpr bool HasSuccessfulImplMethod(char) {
  return false;
}

template <typename ENTRY>
constexpr auto HasSuccessfulImplMethod(int) -> decltype(std::declval<const ENTRY>().SuccessfulImpl(), bool()) {
  return true;
}

template <typename ENTRY>
struct SuccessfulImplMethodTest {
  static constexpr bool value = HasSuccessfulImplMethod<ENTRY>(0);
};

// Whether an `CheckIntegrityImpl()` method is defined for a type.
template <typename ENTRY>
constexpr bool HasCheckIntegrityImplMethod(char) {
  return false;
}

template <typename ENTRY>
constexpr auto HasCheckIntegrityImplMethod(int) -> decltype(std::declval<const ENTRY>().CheckIntegrityImpl(), bool()) {
  return true;
}

template <typename ENTRY>
struct CheckIntegrityImplMethodTest {
  static constexpr bool value = HasCheckIntegrityImplMethod<ENTRY>(0);
};

// Whether an `CURRENT_EXPORTED_STRUCT_NAME()` method is defined for a type.
template <typename ENTRY>
constexpr bool Has_CURRENT_EXPORTED_STRUCT_NAME_Impl(char) {
  return false;
}

template <typename ENTRY>
constexpr auto Has_CURRENT_EXPORTED_STRUCT_NAME_Impl(int)
    -> decltype(std::declval<const ENTRY>().CURRENT_EXPORTED_STRUCT_NAME(), bool()) {
  return true;
}

template <typename ENTRY>
struct Has_CURRENT_EXPORTED_STRUCT_NAME {
  static constexpr bool value = Has_CURRENT_EXPORTED_STRUCT_NAME_Impl<ENTRY>(0);
};

}  // namespace crnt::sfinae
}  // namespace crnt

namespace current {

using ::crnt::CurrentSuper;
using ::crnt::CurrentSuperDeleter;
using ::crnt::CurrentStruct;
using ::crnt::CurrentVariant;

namespace reflection {

template <typename T>
struct SuperTypeImpl {
  static_assert(IS_CURRENT_STRUCT(T), "`SuperType` must be called with the type defined via `CURRENT_STRUCT` macro.");
  using type = typename T::super_t;
};

template <typename T>
using SuperType = typename SuperTypeImpl<T>::type;

template <typename T, typename INTERNAL>
struct TemplateInnerTypeImplExtractor;

template <typename T>
struct TemplateInnerTypeImplExtractor<T, std::false_type> {
  using type = void;
};

template <typename T>
struct TemplateInnerTypeImplExtractor<T, std::true_type> {
  using type = typename T::template_inner_t_impl;
};

template <typename T>
struct TemplateInnerTypeImpl {
  static_assert(IS_CURRENT_STRUCT(T),
                "`TemplateInnerType` must be called with the type defined via `CURRENT_STRUCT` macro.");
  using type = typename TemplateInnerTypeImplExtractor<T, typename T::template_inner_t_internal>::type;
};

template <typename T>
using TemplateInnerType = typename TemplateInnerTypeImpl<T>::type;

enum class FieldCounterPolicy { ThisStructOnly, IncludingSuperFields };

template <typename T, FieldCounterPolicy POLICY = FieldCounterPolicy::ThisStructOnly>
struct FieldCounter {
  static_assert(IS_CURRENT_STRUCT(T),
                "`FieldCounter` must be called with the type defined via `CURRENT_STRUCT` macro.");
  constexpr static size_t value =
      (POLICY == FieldCounterPolicy::ThisStructOnly)
          ? (sizeof(typename T::CURRENT_FIELD_COUNT_STRUCT) / sizeof(CountFieldsImplementationType))
          : (sizeof(typename T::CURRENT_FIELD_COUNT_STRUCT) / sizeof(CountFieldsImplementationType)) +
                FieldCounter<SuperType<T>, POLICY>::value;
};

template <FieldCounterPolicy POLICY>
struct FieldCounter<::crnt::CurrentStruct, POLICY> {
  constexpr static size_t value = 0u;
};

template <typename T>
using TotalFieldCounter = FieldCounter<T, FieldCounterPolicy::IncludingSuperFields>;

struct FieldDescriptions {
  using c_string_t = const char*;
  template <typename T, int I>
  static constexpr c_string_t DescriptionImpl(char) {
    return nullptr;
  }
  template <typename T, int I>
  static constexpr auto DescriptionImpl(int)
      -> decltype(T::CURRENT_REFLECTION_FIELD_DESCRIPTION(SimpleIndex<I>()), c_string_t()) {
    return T::CURRENT_REFLECTION_FIELD_DESCRIPTION(SimpleIndex<I>());
  }
  template <typename T, int I>
  static const char* Description() {
    return DescriptionImpl<T, I>(0);
  }
};

template <typename T, bool IS_STRUCT>
struct IsEmptyCurrentStruct {
  constexpr static bool value = false;
};

template <typename T>
struct IsEmptyCurrentStruct<T, true> {
  constexpr static bool value = (FieldCounter<T>::value == 0);
};

template <typename T, typename VISITOR_TYPE>
struct VisitAllFields {
  static_assert(IS_CURRENT_STRUCT(T),
                "`VisitAllFields` must be called with the type defined via `CURRENT_STRUCT` macro.");
  typedef current::variadic_indexes::generate_indexes<FieldCounter<T>::value> NUM_INDEXES;

  // Visit all fields without an object. Used for enumerating fields and generating signatures.
  template <typename F>
  static void WithoutObject(F&& f) {
    static NUM_INDEXES all_indexes;
    WithoutObjectImpl(std::forward<F>(f), all_indexes);
  }

  // Visit all fields with an object, const or mutable. Used for serialization.
  // Make sure `VisitAllFields<Base, ...>(derived)` treats the passed object as `Base`.
  // Using xvalue reference `TT&&` does not do it, as instead of type `T` passed in
  // as a template parameter to `VisitAllFields<>`, the passed in `t` would be reflected
  // as an object of type `TT`.
  // So, I copy-pasted three implementations for now. -- D.K.
  template <typename F>
  static void WithObject(T& t, F&& f) {
    static NUM_INDEXES all_indexes;
    WithObjectImpl(t, std::forward<F>(f), all_indexes);
  }
  template <typename F>
  static void WithObject(const T& t, F&& f) {
    static NUM_INDEXES all_indexes;
    WithObjectImpl(t, std::forward<F>(f), all_indexes);
  }
  template <typename F>
  static void WithObject(T&& t, F&& f) {
    static NUM_INDEXES all_indexes;
    WithObjectImpl(t, std::forward<F>(f), all_indexes);
  }

 private:
  template <typename F, int N, int... NS>
  static void WithoutObjectImpl(F&& f, current::variadic_indexes::indexes<N, NS...>) {
    static current::variadic_indexes::indexes<NS...> remaining_indexes;
    T::CURRENT_REFLECTION(std::forward<F>(f), Index<VISITOR_TYPE, N>());
    WithoutObjectImpl(std::forward<F>(f), remaining_indexes);
  }

  template <typename F>
  static void WithoutObjectImpl(const F&, current::variadic_indexes::indexes<>) {}

  template <typename TT, typename F, int N, int... NS>
  static void WithObjectImpl(TT&& t, F&& f, current::variadic_indexes::indexes<N, NS...>) {
    static current::variadic_indexes::indexes<NS...> remaining_indexes;
    t.CURRENT_REFLECTION(std::forward<F>(f), Index<VISITOR_TYPE, N>());
    // `WithObjectImpl()` is called only from `WithObject()`, and by this point `TT` is `T`.
    static_assert(std::is_same_v<current::decay_t<TT>, T>, "");  // To be on the safe side.
    WithObjectImpl(std::forward<TT>(t), std::forward<F>(f), remaining_indexes);
  }

  template <typename TT, typename F>
  static void WithObjectImpl(TT&&, const F&, crnt::vi::is<>) {}
};

}  // namespace current::reflection

namespace sfinae {
using ::crnt::sfinae::HasExistsImplMethod;
using ::crnt::sfinae::ValueImplMethodTest;
using ::crnt::sfinae::HasCheckIntegrityImplMethod;
using ::crnt::sfinae::Has_CURRENT_EXPORTED_STRUCT_NAME;
}  // namespace current::sfinae

}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_TYPES_H
