/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_TYPE_SYSTEM_STRUCT_H
#define CURRENT_TYPE_SYSTEM_STRUCT_H

#include "../port.h"

#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "base.h"
#include "helpers.h"
#include "polymorphic.h"

#include "../Bricks/template/decay.h"
#include "../Bricks/template/variadic_indexes.h"

namespace current {
namespace reflection {

template <typename INSTANTIATION_TYPE, typename T>
struct BaseTypeHelperImpl {
  struct type {};
};

template <typename T>
struct BaseTypeHelperImpl<DeclareFields, T> {
  typedef T type;
};

template <typename T>
struct BaseTypeHelperImpl<DeclareStrippedFields, T> {
  typedef T type;
};

template <typename INSTANTIATION_TYPE, typename T>
using BaseTypeHelper = typename BaseTypeHelperImpl<INSTANTIATION_TYPE, T>::type;

// Fields declaration and counting.
template <typename INSTANTIATION_TYPE, typename T>
struct FieldImpl;

template <typename T>
struct FieldImpl<DeclareFields, T> {
  typedef T type;
};

template <typename T>
struct FieldImpl<DeclareStrippedFields, T> {
  typedef Stripped<decay<T>> type;
};

template <typename T>
struct FieldImpl<CountFields, T> {
  // TODO: Read on padding.
  typedef CountFieldsImplementationType type;
};

template <typename INSTANTIATION_TYPE, typename T>
using Field = typename FieldImpl<INSTANTIATION_TYPE, T>::type;

// Check if `CURRENT_REFLECTION(f, Index<FieldType, i>)` can be called for i = [0, N).
template <typename T, int N>
struct CurrentStructFieldsConsistency {
  struct Dummy {};
  constexpr static bool CheckField(char) { return false; }
  constexpr static auto CheckField(int)
      -> decltype(T::CURRENT_REFLECTION(Dummy(), Index<FieldTypeAndName, N>()), bool()) {
    return true;
  }
  constexpr static bool Check() { return CheckField(0) && CurrentStructFieldsConsistency<T, N - 1>::Check(); }
};

template <typename T>
struct CurrentStructFieldsConsistency<T, -1> {
  constexpr static bool Check() { return true; }
};

template <typename T>
struct WithoutParentheses;

template <typename T>
struct WithoutParentheses<int(T)> {
  typedef T result;
};

}  // namespace reflection
}  // namespace current

#define CURRENT_EXPAND_MACRO_IMPL(x) x
#define CURRENT_EXPAND_MACRO(x) CURRENT_EXPAND_MACRO_IMPL(x)

// Macros for structure definition.
#define CURRENT_STRUCT_SWITCH(_1, _2, F, ...) F
#define CURRENT_STRUCT(...) \
  CURRENT_STRUCT_SWITCH(__VA_ARGS__, CURRENT_STRUCT_DERIVED, CURRENT_STRUCT_NOT_DERIVED)(__VA_ARGS__)

#define CURRENT_STRUCT_HELPERS(s, super)                                                            \
  template <typename INSTANTIATION_TYPE>                                                            \
  struct CURRENT_STRUCT_IMPL_##s;                                                                   \
  using s = CURRENT_STRUCT_IMPL_##s<::current::reflection::DeclareFields>;                          \
  using CURRENT_PROTO_##s = CURRENT_STRUCT_IMPL_##s<::current::reflection::DeclareStrippedFields>;  \
  template <typename T>                                                                             \
  struct CURRENT_REFLECTION_HELPER;                                                                 \
  template <>                                                                                       \
  struct CURRENT_REFLECTION_HELPER<s> {                                                             \
    using SUPER = super;                                                                            \
    using STRIPPED_TYPE = CURRENT_PROTO_##s;                                                        \
    using UNSTRIPPED_TYPE = s;                                                                      \
    constexpr static const char* CURRENT_STRUCT_NAME() { return #s; }                               \
    constexpr static size_t CURRENT_FIELD_INDEX_BASE = __COUNTER__ + 1;                             \
    typedef CURRENT_STRUCT_IMPL_##s<::current::reflection::CountFields> CURRENT_FIELD_COUNT_STRUCT; \
  };                                                                                                \
  template <>                                                                                       \
  struct CURRENT_REFLECTION_HELPER<CURRENT_PROTO_##s> {                                             \
    using SUPER = super;                                                                            \
    using STRIPPED_TYPE = CURRENT_PROTO_##s;                                                        \
    using UNSTRIPPED_TYPE = s;                                                                      \
    constexpr static const char* CURRENT_STRUCT_NAME() { return #s; }                               \
    constexpr static size_t CURRENT_FIELD_INDEX_BASE = __COUNTER__;                                 \
    typedef CURRENT_STRUCT_IMPL_##s<::current::reflection::CountFields> CURRENT_FIELD_COUNT_STRUCT; \
  }

#define CURRENT_STRUCT_NOT_DERIVED(s)                 \
  CURRENT_STRUCT_HELPERS(s, ::current::CurrentSuper); \
  template <typename INSTANTIATION_TYPE>              \
  struct CURRENT_STRUCT_IMPL_##s                      \
      : CURRENT_REFLECTION_HELPER<s>,                 \
        ::current::reflection::BaseTypeHelper<INSTANTIATION_TYPE, ::current::CurrentSuper>

#define CURRENT_STRUCT_DERIVED(s, base)                                                  \
  static_assert(IS_CURRENT_STRUCT(base), #base " must be derived from `CurrentSuper`."); \
  CURRENT_STRUCT_HELPERS(s, base);                                                       \
  template <typename INSTANTIATION_TYPE>                                                 \
  struct CURRENT_STRUCT_IMPL_##s : CURRENT_REFLECTION_HELPER<s>,                         \
                                   ::current::reflection::BaseTypeHelper<INSTANTIATION_TYPE, base>

#define CURRENT_FIELD_SWITCH(_1, _2, _3, F, ...) F
#define CURRENT_FIELD(...) \
  CURRENT_FIELD_SWITCH(__VA_ARGS__, CURRENT_FIELD_WITH_VALUE, CURRENT_FIELD_WITH_NO_VALUE)(__VA_ARGS__)

#define CURRENT_FIELD_WITH_NO_VALUE(name, type)                                                             \
  ::current::reflection::Field<INSTANTIATION_TYPE,                                                          \
                               typename ::current::reflection::WithoutParentheses<int(type)>::result> name; \
  CURRENT_FIELD_REFLECTION(CURRENT_EXPAND_MACRO(__COUNTER__) - CURRENT_FIELD_INDEX_BASE - 1, type, name)

#define CURRENT_FIELD_WITH_VALUE(name, type, value)                                                         \
  ::current::reflection::Field<INSTANTIATION_TYPE,                                                          \
                               typename ::current::reflection::WithoutParentheses<int(type)>::result> name{ \
      value};                                                                                               \
  CURRENT_FIELD_REFLECTION(CURRENT_EXPAND_MACRO(__COUNTER__) - CURRENT_FIELD_INDEX_BASE - 1, type, name)

#define CURRENT_TIMESTAMP(field)      \
  template <typename F>               \
  void ReportTimestamp(F&& f) const { \
    f(field);                         \
  }                                   \
  template <typename F>               \
  void ReportTimestamp(F&& f) {       \
    f(field);                         \
  }

#define CURRENT_FIELD_REFLECTION(idx, type, name)                                                              \
  template <class F>                                                                                           \
  static void CURRENT_REFLECTION(F&& CURRENT_CALL_F,                                                           \
                                 ::current::reflection::Index<::current::reflection::FieldTypeAndName, idx>) { \
    CURRENT_CALL_F(::current::reflection::TypeSelector<                                                        \
                       typename ::current::reflection::WithoutParentheses<int(type)>::result>(),               \
                   #name);                                                                                     \
  }                                                                                                            \
  template <class F>                                                                                           \
  void CURRENT_REFLECTION(                                                                                     \
      F&& CURRENT_CALL_F,                                                                                      \
      ::current::reflection::Index<::current::reflection::FieldNameAndImmutableValue, idx>) const {            \
    CURRENT_CALL_F(#name, name);                                                                               \
  }                                                                                                            \
  template <class F>                                                                                           \
  void CURRENT_REFLECTION(                                                                                     \
      F&& CURRENT_CALL_F,                                                                                      \
      ::current::reflection::Index<::current::reflection::FieldNameAndMutableValue, idx>) {                    \
    CURRENT_CALL_F(#name, name);                                                                               \
  }

#define CURRENT_CONSTRUCTOR(s)                                                                               \
  template <typename INSTANTIATION_TYPE_IMPL = INSTANTIATION_TYPE,                                           \
            class = ENABLE_IF<                                                                               \
                std::is_same<INSTANTIATION_TYPE_IMPL, ::current::reflection::DeclareFields>::value ||        \
                std::is_same<INSTANTIATION_TYPE_IMPL, ::current::reflection::DeclareStrippedFields>::value>> \
  CURRENT_STRUCT_IMPL_##s

#define CURRENT_DEFAULT_CONSTRUCTOR(s) CURRENT_CONSTRUCTOR(s)()

#define IS_VALID_CURRENT_STRUCT(s)                                                                      \
  ::current::reflection::CurrentStructFieldsConsistency<s,                                              \
                                                        ::current::reflection::FieldCounter<s>::value - \
                                                            1>::Check()

namespace current {
namespace reflection {

template <typename T>
struct SuperTypeImpl {
  static_assert(IS_CURRENT_STRUCT(T),
                "`SuperType` must be called with the type defined via `CURRENT_STRUCT` macro.");
  typedef typename T::template CURRENT_REFLECTION_HELPER<T>::SUPER type;
};

template <typename T>
using SuperType = typename SuperTypeImpl<T>::type;

template <typename T>
struct FieldCounter {
  static_assert(IS_CURRENT_STRUCT(T),
                "`FieldCounter` must be called with the type defined via `CURRENT_STRUCT` macro.");
  enum {
    value = (sizeof(typename T::template CURRENT_REFLECTION_HELPER<T>::CURRENT_FIELD_COUNT_STRUCT) /
             sizeof(CountFieldsImplementationType))
  };
};

template <typename T>
ENABLE_IF<IS_CURRENT_STRUCT(T), const char*> StructName() {
  return T::template CURRENT_REFLECTION_HELPER<T>::CURRENT_STRUCT_NAME();
}

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
  template <typename TT, typename F>
  static void WithObject(TT&& t, F&& f) {
    static NUM_INDEXES all_indexes;
    WithObjectImpl(std::forward<TT>(t), std::forward<F>(f), all_indexes);
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
    WithObjectImpl(std::forward<TT>(t), std::forward<F>(f), remaining_indexes);
  }

  template <typename TT, typename F>
  static void WithObjectImpl(TT&&, const F&, current::variadic_indexes::indexes<>) {}
};

}  // namespace reflection

}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_STRUCT_H
