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
#include "variant.h"

#include "../Bricks/template/decay.h"
#include "../Bricks/template/variadic_indexes.h"

namespace current {
namespace reflection {

template <typename INSTANTIATION_TYPE, typename T>
struct SUPER_SELECTOR {
  struct type {};
};

template <typename T>
struct SUPER_SELECTOR<DeclareFields, T> {
  typedef T type;
};

template <typename REFLECTION_HELPER, typename INSTANTIATION_TYPE, typename T, typename TEMPLATE_INNER>
#ifndef CURRENT_WINDOWS
struct SUPER : SUPER_SELECTOR<INSTANTIATION_TYPE, T>::type {
#else
struct SUPER : REFLECTION_HELPER, SUPER_SELECTOR<INSTANTIATION_TYPE, T>::type {
#endif
  using super_t = typename SUPER_SELECTOR<INSTANTIATION_TYPE, T>::type;

  SUPER() = default;
  SUPER(const super_t& rhs) : super_t(rhs) {}
  SUPER(super_t&& rhs) : super_t(std::move(rhs)) {}
  using super_t::super_t;

#ifndef CURRENT_WINDOWS
  using template_inner_t_internal =
      TEMPLATE_INNER;  // To extract `Bar` from `CURRENT_STRUCT_T(Foo)` => `Foo<Bar>`.
#else
  using template_inner_t = TEMPLATE_INNER;  // To extract `Bar` from `CURRENT_STRUCT_T(Foo)` => `Foo<Bar>`.
#endif
  constexpr static const char* CURRENT_STRUCT_NAME() { return REFLECTION_HELPER::CURRENT_STRUCT_NAME(); }
  using CURRENT_FIELD_COUNT_STRUCT = typename REFLECTION_HELPER::CURRENT_FIELD_COUNT_STRUCT;
  static const char* CURRENT_REFLECTION_FIELD_DESCRIPTION(...) { return nullptr; }
#ifdef CURRENT_WINDOWS
  using FIELD_INDEX_BASE = typename REFLECTION_HELPER::FIELD_INDEX_BASE;
#endif
};

template <typename T>
struct FIELD_INDEX_BASE_IMPL {
  constexpr static size_t CURRENT_FIELD_INDEX_BASE = T::CURRENT_FIELD_INDEX_BASE_IMPL;
};

// Fields declaration and counting.
template <typename INSTANTIATION_TYPE, typename T>
struct FieldImpl;

template <typename T>
struct FieldImpl<DeclareFields, T> {
  typedef T type;
};

template <typename T>
struct FieldImpl<CountFields, T> {
  // TODO: Read on padding.
  typedef CountFieldsImplementationType type;
};

template <typename INSTANTIATION_TYPE, typename T>
using Field = typename FieldImpl<INSTANTIATION_TYPE, T>::type;

// Check if `CURRENT_REFLECTION(f, Index<FieldType, i>)` can be called for i = [1, N].
template <typename T, size_t N>
struct CurrentStructFieldsConsistency {
  struct Dummy {};
  constexpr static bool CheckField(char) { return false; }
  constexpr static auto CheckField(int)
      -> decltype(T::CURRENT_REFLECTION(Dummy(), Index<FieldTypeAndName, N - 1u>()), bool()) {
    return true;
  }
  constexpr static bool Check() { return CheckField(0) && CurrentStructFieldsConsistency<T, N - 1u>::Check(); }
};

template <typename T>
struct CurrentStructFieldsConsistency<T, 0u> {
  constexpr static bool Check() { return true; }
};

}  // namespace reflection
}  // namespace current

#define CURRENT_EXPAND_MACRO_IMPL(x) x
#define CURRENT_EXPAND_MACRO(x) CURRENT_EXPAND_MACRO_IMPL(x)

// MSVS-friendly macros for structure definition.
#define CS_IMPL1(a) CURRENT_STRUCT_NOT_DERIVED(a)
#define CS_IMPL2(a, b) CURRENT_STRUCT_DERIVED(a, b)

#define CS_N_ARGS_IMPL2(_1, _2, n, ...) n
#define CS_N_ARGS_IMPL(args) CS_N_ARGS_IMPL2 args

#define CS_NARGS(...) CS_N_ARGS_IMPL((__VA_ARGS__, 2, 1, 0))

#define CS_CHOOSER2(n) CS_IMPL##n
#define CS_CHOOSER1(n) CS_CHOOSER2(n)
#define CS_CHOOSERX(n) CS_CHOOSER1(n)

#define CS_SWITCH(x, y) x y
#define CURRENT_STRUCT(...) CS_SWITCH(CS_CHOOSERX(CS_NARGS(__VA_ARGS__)), (__VA_ARGS__))

#define CST_IMPL1(a) CURRENT_STRUCT_T_NOT_DERIVED(a)
#define CST_IMPL2(a, b) CURRENT_STRUCT_T_DERIVED(a, b)

#define CST_N_ARGS_IMPL2(_1, _2, n, ...) n
#define CST_N_ARGS_IMPL(args) CST_N_ARGS_IMPL2 args

#define CST_NARGS(...) CST_N_ARGS_IMPL((__VA_ARGS__, 2, 1, 0))

#define CST_CHOOSER2(n) CST_IMPL##n
#define CST_CHOOSER1(n) CST_CHOOSER2(n)
#define CST_CHOOSERX(n) CST_CHOOSER1(n)

#define CST_SWITCH(x, y) x y
#define CURRENT_STRUCT_T(...) CST_SWITCH(CST_CHOOSERX(CST_NARGS(__VA_ARGS__)), (__VA_ARGS__))

// Current structure forward declaration.
#define CURRENT_FORWARD_DECLARE_STRUCT(s) \
  template <typename T>                   \
  struct CURRENT_STRUCT_IMPL_##s;         \
  using s = CURRENT_STRUCT_IMPL_##s<::current::reflection::DeclareFields>

// Current structure implementation.
#ifndef CURRENT_WINDOWS

#define CURRENT_STRUCT_HELPERS(s, super)                                                                   \
  template <typename INSTANTIATION_TYPE>                                                                   \
  struct CURRENT_STRUCT_IMPL_##s;                                                                          \
  using s = CURRENT_STRUCT_IMPL_##s<::current::reflection::DeclareFields>;                                 \
  template <typename>                                                                                      \
  struct CURRENT_REFLECTION_HELPER;                                                                        \
  template <>                                                                                              \
  struct CURRENT_REFLECTION_HELPER<s> {                                                                    \
    constexpr static size_t CURRENT_FIELD_INDEX_BASE_IMPL = __COUNTER__;                                   \
    constexpr static const char* CURRENT_STRUCT_NAME() { return #s; }                                      \
    typedef CURRENT_STRUCT_IMPL_##s<::current::reflection::CountFields> CURRENT_FIELD_COUNT_STRUCT;        \
    using FIELD_INDEX_BASE = ::current::reflection::FIELD_INDEX_BASE_IMPL<CURRENT_REFLECTION_HELPER<s>>;   \
  };                                                                                                       \
  struct CURRENT_STRUCT_SUPER_HELPER_##s {                                                                 \
    using FIELD_INDEX_BASE = typename CURRENT_REFLECTION_HELPER<s>::FIELD_INDEX_BASE;                      \
    using SUPER = ::current::reflection::                                                                  \
        SUPER<CURRENT_REFLECTION_HELPER<s>, ::current::reflection::DeclareFields, super, std::false_type>; \
    using template_inner_t_0 = void;                                                                       \
  }

#define CURRENT_STRUCT_T_HELPERS(s, super)                                                                  \
  template <typename T, typename INSTANTIATION_TYPE>                                                        \
  struct CURRENT_STRUCT_T_IMPL_##s;                                                                         \
  template <typename T>                                                                                     \
  using s = CURRENT_STRUCT_T_IMPL_##s<T, ::current::reflection::DeclareFields>;                             \
  template <template <typename> class>                                                                      \
  struct CURRENT_REFLECTION_T_HELPER;                                                                       \
  template <>                                                                                               \
  struct CURRENT_REFLECTION_T_HELPER<s> {                                                                   \
    constexpr static size_t CURRENT_FIELD_INDEX_BASE_IMPL = __COUNTER__;                                    \
    constexpr static const char* CURRENT_STRUCT_NAME() { return #s "<>"; }                                  \
    typedef CURRENT_STRUCT_T_IMPL_##s<int, ::current::reflection::CountFields> CURRENT_FIELD_COUNT_STRUCT;  \
    using FIELD_INDEX_BASE = ::current::reflection::FIELD_INDEX_BASE_IMPL<CURRENT_REFLECTION_T_HELPER<s>>;  \
  };                                                                                                        \
  struct CURRENT_STRUCT_T_SUPER_HELPER_##s {                                                                \
    using FIELD_INDEX_BASE = typename CURRENT_REFLECTION_T_HELPER<s>::FIELD_INDEX_BASE;                     \
    using SUPER = ::current::reflection::                                                                   \
        SUPER<CURRENT_REFLECTION_T_HELPER<s>, ::current::reflection::DeclareFields, super, std::true_type>; \
  };                                                                                                        \
  template <typename T>                                                                                     \
  struct CURRENT_STRUCT_T_TEMPLATE_INNER_TYPE_EXTRACTOR_##s {                                               \
    using template_inner_t_impl = T;                                                                        \
  }

#else  // CURRENT_WINDOWS

#define CURRENT_STRUCT_HELPERS(s, super)                                                                 \
  template <typename INSTANTIATION_TYPE>                                                                 \
  struct CURRENT_STRUCT_IMPL_##s;                                                                        \
  using s = CURRENT_STRUCT_IMPL_##s<::current::reflection::DeclareFields>;                               \
  template <typename>                                                                                    \
  struct CURRENT_REFLECTION_HELPER;                                                                      \
  template <>                                                                                            \
  struct CURRENT_REFLECTION_HELPER<s> {                                                                  \
    constexpr static size_t CURRENT_FIELD_INDEX_BASE_IMPL = __COUNTER__;                                 \
    constexpr static const char* CURRENT_STRUCT_NAME() { return #s; }                                    \
    typedef CURRENT_STRUCT_IMPL_##s<::current::reflection::CountFields> CURRENT_FIELD_COUNT_STRUCT;      \
    using FIELD_INDEX_BASE = ::current::reflection::FIELD_INDEX_BASE_IMPL<CURRENT_REFLECTION_HELPER<s>>; \
  }

#define CURRENT_STRUCT_T_HELPERS(s, super)                                                                 \
  template <typename T, typename INSTANTIATION_TYPE>                                                       \
  struct CURRENT_STRUCT_T_IMPL_##s;                                                                        \
  template <typename T>                                                                                    \
  using s = CURRENT_STRUCT_T_IMPL_##s<T, ::current::reflection::DeclareFields>;                            \
  template <template <typename> class>                                                                     \
  struct CURRENT_REFLECTION_T_HELPER;                                                                      \
  template <>                                                                                              \
  struct CURRENT_REFLECTION_T_HELPER<s> {                                                                  \
    constexpr static size_t CURRENT_FIELD_INDEX_BASE_IMPL = __COUNTER__;                                   \
    constexpr static const char* CURRENT_STRUCT_NAME() { return #s "<>"; }                                 \
    typedef CURRENT_STRUCT_T_IMPL_##s<int, ::current::reflection::CountFields> CURRENT_FIELD_COUNT_STRUCT; \
    using FIELD_INDEX_BASE = ::current::reflection::FIELD_INDEX_BASE_IMPL<CURRENT_REFLECTION_T_HELPER<s>>; \
  }

#endif  // CURRENT_WINDOWS

// `CURRENT_STRUCT` implementations need to extract `CURRENT_FIELD_INDEX_BASE`,
// and its scope resolution for derived structs differs between Visual C++ and g++/clang++. -- D.K.

#ifndef CURRENT_WINDOWS

#define CURRENT_STRUCT_NOT_DERIVED(s)                  \
  CURRENT_STRUCT_HELPERS(s, ::current::CurrentStruct); \
  template <typename INSTANTIATION_TYPE>               \
  struct CURRENT_STRUCT_IMPL_##s                       \
      : CURRENT_STRUCT_SUPER_HELPER_##s,               \
        ::current::reflection::                        \
            SUPER<CURRENT_REFLECTION_HELPER<s>, INSTANTIATION_TYPE, ::current::CurrentStruct, std::false_type>

#define CURRENT_STRUCT_T_NOT_DERIVED(s)                                                           \
  CURRENT_STRUCT_T_HELPERS(s, ::current::CurrentStruct);                                          \
  template <typename T, typename INSTANTIATION_TYPE>                                              \
  struct CURRENT_STRUCT_T_IMPL_##s : CURRENT_STRUCT_T_SUPER_HELPER_##s,                           \
                                     CURRENT_STRUCT_T_TEMPLATE_INNER_TYPE_EXTRACTOR_##s<T>,       \
                                     ::current::reflection::SUPER<CURRENT_REFLECTION_T_HELPER<s>, \
                                                                  INSTANTIATION_TYPE,             \
                                                                  ::current::CurrentStruct,       \
                                                                  std::true_type>

#define CURRENT_STRUCT_DERIVED(s, base)                                                   \
  static_assert(IS_CURRENT_STRUCT(base), #base " must be derived from `CurrentStruct`."); \
  CURRENT_STRUCT_HELPERS(s, base);                                                        \
  template <typename INSTANTIATION_TYPE>                                                  \
  struct CURRENT_STRUCT_IMPL_##s                                                          \
      : CURRENT_STRUCT_SUPER_HELPER_##s,                                                  \
        ::current::reflection::SUPER<CURRENT_REFLECTION_HELPER<s>, INSTANTIATION_TYPE, base, std::false_type>

// TODO(dkorolev): I've sacrificed the `static_assert(IS_CURRENT_STRUCT(base), #base " must be ...");` for now.
// TODO(dkorolev): It can be re-inserted back via a helper class to inherit from. Not now. -- D.K.
// ===>  static_assert(IS_CURRENT_STRUCT(base), #base " must be derived from `CurrentStruct`.");  <===
#define CURRENT_STRUCT_T_DERIVED(s, base)                      \
  CURRENT_STRUCT_T_HELPERS(s, base);                           \
  template <typename T, typename INSTANTIATION_TYPE>           \
  struct CURRENT_STRUCT_T_IMPL_##s                             \
      : CURRENT_STRUCT_T_SUPER_HELPER_##s,                     \
        CURRENT_STRUCT_T_TEMPLATE_INNER_TYPE_EXTRACTOR_##s<T>, \
        ::current::reflection::SUPER<CURRENT_REFLECTION_T_HELPER<s>, INSTANTIATION_TYPE, base, std::true_type>

#else  // CURRENT_WINDOWS

#define CURRENT_STRUCT_NOT_DERIVED(s)                  \
  CURRENT_STRUCT_HELPERS(s, ::current::CurrentStruct); \
  template <typename INSTANTIATION_TYPE>               \
  struct CURRENT_STRUCT_IMPL_##s                       \
      : ::current::reflection::                        \
            SUPER<CURRENT_REFLECTION_HELPER<s>, INSTANTIATION_TYPE, ::current::CurrentStruct, void>

#define CURRENT_STRUCT_T_NOT_DERIVED(s)                  \
  CURRENT_STRUCT_T_HELPERS(s, ::current::CurrentStruct); \
  template <typename T, typename INSTANTIATION_TYPE>     \
  struct CURRENT_STRUCT_T_IMPL_##s                       \
      : ::current::reflection::                          \
            SUPER<CURRENT_REFLECTION_T_HELPER<s>, INSTANTIATION_TYPE, ::current::CurrentStruct, T>

#define CURRENT_STRUCT_DERIVED(s, base)                                                   \
  static_assert(IS_CURRENT_STRUCT(base), #base " must be derived from `CurrentStruct`."); \
  CURRENT_STRUCT_HELPERS(s, base);                                                        \
  template <typename INSTANTIATION_TYPE>                                                  \
  struct CURRENT_STRUCT_IMPL_##s                                                          \
      : ::current::reflection::SUPER<CURRENT_REFLECTION_HELPER<s>, INSTANTIATION_TYPE, base, void>

// TODO(dkorolev): I've sacrificed the `static_assert(IS_CURRENT_STRUCT(base), #base " must be ...");` for now.
// TODO(dkorolev): It can be re-inserted back via a helper class to inherit from. Not now. -- D.K.
// ===>  static_assert(IS_CURRENT_STRUCT(base), #base " must be derived from `CurrentStruct`.");  <===

#define CURRENT_STRUCT_T_DERIVED(s, base)            \
  CURRENT_STRUCT_T_HELPERS(s, base);                 \
  template <typename T, typename INSTANTIATION_TYPE> \
  struct CURRENT_STRUCT_T_IMPL_##s                   \
      : ::current::reflection::SUPER<CURRENT_REFLECTION_T_HELPER<s>, INSTANTIATION_TYPE, base, T>

#endif  // CURRENT_WINDOWS

// Macro to equally treat bare and parenthesized type arguments:
// CF_TYPE((int)) = CF_TYPE(int) = int
#define EMPTY_CF_TYPE_EXTRACT
#define CF_TYPE_PASTE(x, ...) x##__VA_ARGS__
#define CF_TYPE_PASTE2(x, ...) CF_TYPE_PASTE(x, __VA_ARGS__)
#define CF_TYPE_EXTRACT(...) CF_TYPE_EXTRACT __VA_ARGS__
#define CF_TYPE(x) CF_TYPE_PASTE2(EMPTY_, CF_TYPE_EXTRACT x)

// `CURRENT_FIELD` related macros and implementation.
#define CF_IMPL2(a, b) CURRENT_FIELD_WITH_NO_VALUE(a, b)
#define CF_IMPL3(a, b, c) CURRENT_FIELD_WITH_VALUE(a, b, c)

#define CF_N_ARGS_IMPL3(_1, _2, _3, n, ...) n
#define CF_NARGS_IMPL(args) CF_N_ARGS_IMPL3 args

#define CF_NARGS(...) CF_NARGS_IMPL((__VA_ARGS__, 3, 2, 1, 0))

#define CF_CHOOSER2(n) CF_IMPL##n
#define CF_CHOOSER1(n) CF_CHOOSER2(n)
#define CF_CHOOSERX(n) CF_CHOOSER1(n)

#define CF_SWITCH(x, y) x y
#define CURRENT_FIELD(...) CF_SWITCH(CF_CHOOSERX(CF_NARGS(__VA_ARGS__)), (__VA_ARGS__))

#define CURRENT_FIELD_WITH_NO_VALUE(name, type)                                           \
  ::current::reflection::Field<INSTANTIATION_TYPE, CF_TYPE(type)> name;                   \
  constexpr static size_t CURRENT_FIELD_INDEX_##name =                                    \
      CURRENT_EXPAND_MACRO(__COUNTER__) - FIELD_INDEX_BASE::CURRENT_FIELD_INDEX_BASE - 1; \
  CURRENT_FIELD_REFLECTION(CURRENT_FIELD_INDEX_##name, type, name)

#define CURRENT_FIELD_WITH_VALUE(name, type, value)                                       \
  ::current::reflection::Field<INSTANTIATION_TYPE, CF_TYPE(type)> name{CF_TYPE(value)};   \
  constexpr static size_t CURRENT_FIELD_INDEX_##name =                                    \
      CURRENT_EXPAND_MACRO(__COUNTER__) - FIELD_INDEX_BASE::CURRENT_FIELD_INDEX_BASE - 1; \
  CURRENT_FIELD_REFLECTION(CURRENT_FIELD_INDEX_##name, type, name)

#define CURRENT_FIELD_DESCRIPTION(name, description)                    \
  static const char* CURRENT_REFLECTION_FIELD_DESCRIPTION(              \
      ::current::reflection::SimpleIndex<CURRENT_FIELD_INDEX_##name>) { \
    return description;                                                 \
  }

#define CURRENT_USE_FIELD_AS_KEY(field)                                 \
  using cf_key_t = current::copy_free<decltype(field)>;                 \
  cf_key_t key() const { return field; }                                \
  void set_key(cf_key_t new_key_value) const { field = new_key_value; } \
  using CURRENT_USE_FIELD_AS_KEY_##field##_implemented = void

#define CURRENT_USE_FIELD_AS_ROW(field)                                 \
  using cf_row_t = current::copy_free<decltype(field)>;                 \
  cf_row_t row() const { return field; }                                \
  void set_row(cf_row_t new_row_value) const { field = new_row_value; } \
  using CURRENT_USE_FIELD_AS_ROW_##field##_implemented = void

#define CURRENT_USE_FIELD_AS_COL(field)                                 \
  using cf_col_t = current::copy_free<decltype(field)>;                 \
  cf_col_t col() const { return field; }                                \
  void set_col(cf_col_t new_col_value) const { field = new_col_value; } \
  using CURRENT_USE_FIELD_AS_COL_##field##_implemented = void

#define CURRENT_USE_FIELD_AS_TIMESTAMP(field) \
  template <typename F>                       \
  void ReportTimestamp(F&& f) const {         \
    f(field);                                 \
  }                                           \
  template <typename F>                       \
  void ReportTimestamp(F&& f) {               \
    f(field);                                 \
  }

#define CURRENT_FIELD_REFLECTION(idx, type, name)                                                              \
  template <class F>                                                                                           \
  static void CURRENT_REFLECTION(F&& CURRENT_CALL_F,                                                           \
                                 ::current::reflection::Index<::current::reflection::FieldTypeAndName, idx>) { \
    CURRENT_CALL_F(::current::reflection::TypeSelector<CF_TYPE(type)>(), #name);                               \
  }                                                                                                            \
  template <class F>                                                                                           \
  static void CURRENT_REFLECTION(                                                                              \
      F&& CURRENT_CALL_F,                                                                                      \
      ::current::reflection::Index<::current::reflection::FieldTypeAndNameAndIndex, idx>) {                    \
    CURRENT_CALL_F(::current::reflection::TypeSelector<CF_TYPE(type)>(),                                       \
                   #name,                                                                                      \
                   ::current::reflection::SimpleIndex<idx>());                                                 \
  }                                                                                                            \
  template <class F, class SELF>                                                                               \
  static void CURRENT_REFLECTION(                                                                              \
      F&& CURRENT_CALL_F, ::current::reflection::Index<::current::reflection::FieldNameAndPtr<SELF>, idx>) {   \
    CURRENT_CALL_F(#name, &SELF::name);                                                                        \
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

#define CURRENT_CONSTRUCTOR(s)                                                                                 \
  template <typename INSTANTIATION_TYPE_IMPL = INSTANTIATION_TYPE,                                             \
            class =                                                                                            \
                ENABLE_IF<std::is_same<INSTANTIATION_TYPE_IMPL, ::current::reflection::DeclareFields>::value>> \
  CURRENT_STRUCT_IMPL_##s

#define CURRENT_ASSIGN_OPER(s)                                                                                 \
  template <typename INSTANTIATION_TYPE_IMPL = INSTANTIATION_TYPE,                                             \
            class =                                                                                            \
                ENABLE_IF<std::is_same<INSTANTIATION_TYPE_IMPL, ::current::reflection::DeclareFields>::value>> \
  CURRENT_STRUCT_IMPL_##s& operator=

#define CURRENT_DEFAULT_CONSTRUCTOR(s) CURRENT_CONSTRUCTOR(s)()

#define CURRENT_CONSTRUCTOR_T(s)                                                                               \
  template <typename INSTANTIATION_TYPE_IMPL = INSTANTIATION_TYPE,                                             \
            class =                                                                                            \
                ENABLE_IF<std::is_same<INSTANTIATION_TYPE_IMPL, ::current::reflection::DeclareFields>::value>> \
  CURRENT_STRUCT_T_IMPL_##s

#define CURRENT_DEFAULT_CONSTRUCTOR_T(s) CURRENT_CONSTRUCTOR_T(s)()

#define IS_VALID_CURRENT_STRUCT(s)                                                                          \
  ::current::reflection::CurrentStructFieldsConsistency<s, ::current::reflection::FieldCounter<s>::value>:: \
      Check()

namespace current {
namespace reflection {

template <typename T>
struct SuperTypeImpl {
  static_assert(IS_CURRENT_STRUCT(T),
                "`SuperType` must be called with the type defined via `CURRENT_STRUCT` macro.");
  using type = typename T::super_t;
};

template <typename T>
using SuperType = typename SuperTypeImpl<T>::type;

#ifndef CURRENT_WINDOWS
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
#else
template <typename T>
struct TemplateInnerTypeImpl {
  static_assert(IS_CURRENT_STRUCT(T),
                "`TemplateInnerType` must be called with the type defined via `CURRENT_STRUCT` macro.");
  using type = typename T::template_inner_t;
};
#endif  // CURRENT_WINDOWS

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
struct FieldCounter<CurrentStruct, POLICY> {
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
    static_assert(std::is_same<current::decay<TT>, T>::value, "");  // To be on the safe side.
    WithObjectImpl(std::forward<TT>(t), std::forward<F>(f), remaining_indexes);
  }

  template <typename TT, typename F>
  static void WithObjectImpl(TT&&, const F&, current::variadic_indexes::indexes<>) {}
};

}  // namespace reflection

// TODO(dkorolev): Find a better place for this code. Must be included from the top-level `current.h`.

template <class B, class D>
struct is_same_or_base_of {
  constexpr static bool value = std::is_base_of<B, D>::value;
};
template <class C>
struct is_same_or_base_of<C, C> {
  constexpr static bool value = true;
};

}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_STRUCT_H
