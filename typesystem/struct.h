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

#include "remove_parentheses.h"

#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "base.h"
#include "helpers.h"
#include "variant.h"

namespace crnt {
namespace r {

template <typename INSTANTIATION_TYPE, typename T>
struct SUPER_SELECTOR {
  struct type {};
};

template <typename T>
struct SUPER_SELECTOR<DF, T> {
  typedef T type;
};

template <typename REFLECTION_HELPER, typename INSTANTIATION_TYPE, typename T, typename TEMPLATE_INNER>
struct SUPER_IMPL : SUPER_SELECTOR<INSTANTIATION_TYPE, T>::type {
  using super_t = typename SUPER_SELECTOR<INSTANTIATION_TYPE, T>::type;

  template <class IT = INSTANTIATION_TYPE, class = std::enable_if_t<std::is_same_v<IT, DF>>>
  SUPER_IMPL(const super_t& self) : super_t(self) {}

  template <class IT = INSTANTIATION_TYPE, class = std::enable_if_t<std::is_same_v<IT, DF>>>
  SUPER_IMPL(super_t&& self) : super_t(self) {}

  using super_t::super_t;

  using template_inner_t_internal = TEMPLATE_INNER;  // To extract `Bar` from `CURRENT_STRUCT_T(Foo)` => `Foo<Bar>`.
  constexpr static const char* CURRENT_STRUCT_NAME() { return REFLECTION_HELPER::CURRENT_STRUCT_NAME(); }
  using CURRENT_FIELD_COUNT_STRUCT = typename REFLECTION_HELPER::CURRENT_FIELD_COUNT_STRUCT;
  static const char* CURRENT_REFLECTION_FIELD_DESCRIPTION(...) { return nullptr; }
};

template <typename T>
struct FIELD_INDEX_BASE_IMPL {
  constexpr static size_t CURRENT_FIELD_INDEX_BASE = T::CURRENT_FIELD_INDEX_BASE_IMPL;
};

// Fields declaration and counting.
template <typename INSTANTIATION_TYPE, typename T>
struct FieldImpl;

template <typename T>
struct FieldImpl<DF, T> {
  typedef T type;
};

template <typename T>
struct FieldImpl<FC, T> {
  // TODO: Read on padding.
  typedef CountFieldsImplementationType type;
};

// For compile-time field type by index extraction.
template <typename T>
struct FieldTypeWrapper {
  using type = T;
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

}  // namespace r
}  // namespace crnt

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
  struct CSI_##s;                         \
  using s = CSI_##s<::crnt::r::DF>

// Current structure implementation.
#define CURRENT_STRUCT_HELPERS(s, super)                                           \
  template <typename INSTANTIATION_TYPE>                                           \
  struct CSI_##s;                                                                  \
  using s = CSI_##s<::crnt::r::DF>;                                                \
  struct CRH_##s final {                                                           \
    constexpr static size_t CURRENT_FIELD_INDEX_BASE_IMPL = __COUNTER__;           \
    constexpr static const char* CURRENT_STRUCT_NAME() { return #s; }              \
    typedef CSI_##s<::crnt::r::FC> CURRENT_FIELD_COUNT_STRUCT;                     \
    using FIELD_INDEX_BASE = ::crnt::r::FIELD_INDEX_BASE_IMPL<CRH_##s>;            \
  };                                                                               \
  inline CRH_##s CRHTypeExtractor(::crnt::r::TypeSelector<s>);                     \
  struct CSSH_##s {                                                                \
    using FIELD_INDEX_BASE = typename CRH_##s::FIELD_INDEX_BASE;                   \
    using INTERNAL_SUPER = super;                                                  \
    using template_inner_t_0 = void;                                               \
  }

#define CURRENT_STRUCT_T_HELPERS(s, super)                                         \
  template <typename T, typename INSTANTIATION_TYPE>                               \
  struct CSTI_##s;                                                                 \
  template <typename T>                                                            \
  using s = CSTI_##s<T, ::crnt::r::DF>;                                            \
  template <template <typename> class>                                             \
  struct CRTH;                                                                     \
  template <>                                                                      \
  struct CRTH<s> {                                                                 \
    constexpr static size_t CURRENT_FIELD_INDEX_BASE_IMPL = __COUNTER__;           \
    constexpr static const char* CURRENT_STRUCT_NAME() { return #s "_Z"; }         \
    typedef CSTI_##s<::crnt::r::DummyT, ::crnt::r::FC> CURRENT_FIELD_COUNT_STRUCT; \
    using FIELD_INDEX_BASE = ::crnt::r::FIELD_INDEX_BASE_IMPL<CRTH<s>>;            \
  };                                                                               \
  struct CURRENT_STRUCT_T_SUPER_HELPER_##s {                                       \
    using FIELD_INDEX_BASE = typename CRTH<s>::FIELD_INDEX_BASE;                   \
    using INTERNAL_SUPER_T = super;                                                \
  };                                                                               \
  template <typename T>                                                            \
  struct CURRENT_STRUCT_T_TEMPLATE_INNER_TYPE_EXTRACTOR_##s {                      \
    using template_inner_t_impl = T;                                               \
  }

// `CURRENT_STRUCT` implementations need to extract `CURRENT_FIELD_INDEX_BASE`,
// and its scope resolution for derived structs differs between Visual C++ and g++/clang++. -- D.K.

#define CURRENT_STRUCT_NOT_DERIVED(s)               \
  CURRENT_STRUCT_HELPERS(s, ::crnt::CurrentStruct); \
  template <typename INSTANTIATION_TYPE>            \
  struct CSI_##s : CSSH_##s, ::crnt::r::SUPER_IMPL<CRH_FOR(s), INSTANTIATION_TYPE, ::crnt::CurrentStruct, std::false_type>

#define CURRENT_STRUCT_T_NOT_DERIVED(s)                                    \
  CURRENT_STRUCT_T_HELPERS(s, ::crnt::CurrentStruct);                      \
  template <typename T, typename INSTANTIATION_TYPE>                       \
  struct CSTI_##s : CURRENT_STRUCT_T_SUPER_HELPER_##s,                     \
                    CURRENT_STRUCT_T_TEMPLATE_INNER_TYPE_EXTRACTOR_##s<T>, \
                    ::crnt::r::SUPER_IMPL<CRTH<s>, INSTANTIATION_TYPE, ::crnt::CurrentStruct, std::true_type>

#define CURRENT_STRUCT_DERIVED(s, base)                                                           \
  static_assert(IS_CURRENT_STRUCT(base), #base " must be derived from `::crnt::CurrentStruct`."); \
  CURRENT_STRUCT_HELPERS(s, base);                                                                \
  template <typename INSTANTIATION_TYPE>                                                          \
  struct CSI_##s : CSSH_##s, ::crnt::r::SUPER_IMPL<CRH_FOR(s), INSTANTIATION_TYPE, base, std::false_type>

// TODO(dkorolev): I've sacrificed the `static_assert(IS_CURRENT_STRUCT(base), #base " must be ...");` for now.
// TODO(dkorolev): It can be re-inserted back via a helper class to inherit from. Not now. -- D.K.
// ===>  static_assert(IS_CURRENT_STRUCT(base), #base " must be derived from `CurrentStruct`.");  <===
#define CURRENT_STRUCT_T_DERIVED(s, base)                                  \
  CURRENT_STRUCT_T_HELPERS(s, base);                                       \
  template <typename T, typename INSTANTIATION_TYPE>                       \
  struct CSTI_##s : CURRENT_STRUCT_T_SUPER_HELPER_##s,                     \
                    CURRENT_STRUCT_T_TEMPLATE_INNER_TYPE_EXTRACTOR_##s<T>, \
                    ::crnt::r::SUPER_IMPL<CRTH<s>, INSTANTIATION_TYPE, base, std::true_type>

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

#define CURRENT_FIELD_WITH_NO_VALUE(name, type)                                            \
  ::current::reflection::Field<INSTANTIATION_TYPE, CURRENT_REMOVE_PARENTHESES(type)> name; \
  constexpr static size_t CURRENT_FIELD_INDEX_##name =                                     \
      CURRENT_EXPAND_MACRO(__COUNTER__) - FIELD_INDEX_BASE::CURRENT_FIELD_INDEX_BASE - 1;  \
  CURRENT_FIELD_REFLECTION(CURRENT_FIELD_INDEX_##name, type, name)

#define CURRENT_FIELD_WITH_VALUE(name, type, value)                                        \
  ::current::reflection::Field<INSTANTIATION_TYPE, CURRENT_REMOVE_PARENTHESES(type)> name{ \
      CURRENT_REMOVE_PARENTHESES(value)};                                                  \
  constexpr static size_t CURRENT_FIELD_INDEX_##name =                                     \
      CURRENT_EXPAND_MACRO(__COUNTER__) - FIELD_INDEX_BASE::CURRENT_FIELD_INDEX_BASE - 1;  \
  CURRENT_FIELD_REFLECTION(CURRENT_FIELD_INDEX_##name, type, name)

#define CURRENT_FIELD_DESCRIPTION(name, description)                    \
  static const char* CURRENT_REFLECTION_FIELD_DESCRIPTION(              \
      ::current::reflection::SimpleIndex<CURRENT_FIELD_INDEX_##name>) { \
    return description;                                                 \
  }

#define CURRENT_USE_FIELD_AS_KEY(field)                           \
  using cf_key_t = current::copy_free<decltype(field)>;           \
  cf_key_t key() const { return field; }                          \
  void set_key(cf_key_t new_key_value) { field = new_key_value; } \
  using CURRENT_USE_FIELD_AS_KEY_##field##_implemented = void

#define CURRENT_USE_FIELD_AS_ROW(field)                           \
  using cf_row_t = current::copy_free<decltype(field)>;           \
  cf_row_t row() const { return field; }                          \
  void set_row(cf_row_t new_row_value) { field = new_row_value; } \
  using CURRENT_USE_FIELD_AS_ROW_##field##_implemented = void

#define CURRENT_USE_FIELD_AS_COL(field)                           \
  using cf_col_t = current::copy_free<decltype(field)>;           \
  cf_col_t col() const { return field; }                          \
  void set_col(cf_col_t new_col_value) { field = new_col_value; } \
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

#define CURRENT_FIELD_REFLECTION(idx, type, name)                                                                      \
  template <class F>                                                                                                   \
  static void CURRENT_REFLECTION(F&& CURRENT_CALL_F,                                                                   \
                                 ::current::reflection::Index<::current::reflection::FieldTypeAndName, idx>) {         \
    CURRENT_CALL_F(::current::reflection::TypeSelector<CURRENT_REMOVE_PARENTHESES(type)>(), #name);                    \
  }                                                                                                                    \
  template <class F>                                                                                                   \
  static void CURRENT_REFLECTION(F&& CURRENT_CALL_F,                                                                   \
                                 ::current::reflection::Index<::current::reflection::FieldTypeAndNameAndIndex, idx>) { \
    CURRENT_CALL_F(::current::reflection::TypeSelector<CURRENT_REMOVE_PARENTHESES(type)>(),                            \
                   #name,                                                                                              \
                   ::current::reflection::SimpleIndex<idx>());                                                         \
  }                                                                                                                    \
  template <class F, class SELF>                                                                                       \
  static void CURRENT_REFLECTION(F&& CURRENT_CALL_F,                                                                   \
                                 ::current::reflection::Index<::current::reflection::FieldNameAndPtr<SELF>, idx>) {    \
    CURRENT_CALL_F(#name, &SELF::name);                                                                                \
  }                                                                                                                    \
  template <class F>                                                                                                   \
  void CURRENT_REFLECTION(F&& CURRENT_CALL_F,                                                                          \
                          ::current::reflection::Index<::current::reflection::FieldNameAndImmutableValue, idx>)        \
      const {                                                                                                          \
    CURRENT_CALL_F(#name, name);                                                                                       \
  }                                                                                                                    \
  template <class F>                                                                                                   \
  void CURRENT_REFLECTION(F&& CURRENT_CALL_F,                                                                          \
                          ::current::reflection::Index<::current::reflection::FieldNameAndMutableValue, idx>) {        \
    CURRENT_CALL_F(#name, name);                                                                                       \
  }                                                                                                                    \
  static ::crnt::r::FieldTypeWrapper<CURRENT_REMOVE_PARENTHESES(type)> CURRENT_REFLECTION(                             \
      ::current::reflection::Index<::current::reflection::FieldType, idx>);

#define CURRENT_CONSTRUCTOR(s)                                                                                   \
  template <                                                                                                     \
      class SUPER =                                                                                              \
          ::crnt::r::SUPER_IMPL<CRH_FOR(s), INSTANTIATION_TYPE, typename CSSH_##s::INTERNAL_SUPER, std::false_type>, \
      typename INSTANTIATION_TYPE_IMPL = INSTANTIATION_TYPE,                                                     \
      class = std::enable_if_t<std::is_same_v<INSTANTIATION_TYPE_IMPL, ::current::reflection::DeclareFields>>>   \
  CSI_##s

#define CURRENT_ASSIGN_OPER(s)                                                                                   \
  template <typename INSTANTIATION_TYPE_IMPL = INSTANTIATION_TYPE,                                               \
            class =                                                                                              \
                std::enable_if_t<std::is_same_v<INSTANTIATION_TYPE_IMPL, ::current::reflection::DeclareFields>>> \
  CSI_##s& operator=

#define CURRENT_DEFAULT_CONSTRUCTOR(s) CURRENT_CONSTRUCTOR(s)()

#define CURRENT_CONSTRUCTOR_T(s)                                                                                     \
  template <class SUPER = ::crnt::r::SUPER_IMPL<CRTH<s>,                                                             \
                                                INSTANTIATION_TYPE,                                                  \
                                                typename CURRENT_STRUCT_T_SUPER_HELPER_##s::INTERNAL_SUPER_T,        \
                                                std::true_type>,                                                     \
            typename INSTANTIATION_TYPE_IMPL = INSTANTIATION_TYPE,                                                   \
            class = std::enable_if_t<std::is_same_v<INSTANTIATION_TYPE_IMPL, ::current::reflection::DeclareFields>>> \
  CSTI_##s

#define CURRENT_DEFAULT_CONSTRUCTOR_T(s) CURRENT_CONSTRUCTOR_T(s)()

#define IS_VALID_CURRENT_STRUCT(s) \
  ::current::reflection::CurrentStructFieldsConsistency<s, ::current::reflection::FieldCounter<s>::value>::Check()

#define CURRENT_EXPORTED_TEMPLATED_STRUCT(original_struct_name, original_template_inner_type)        \
  constexpr static const char* CURRENT_EXPORTED_STRUCT_NAME() { return #original_struct_name "_Z"; } \
  using template_inner_t_internal = std::true_type;                                                  \
  using template_inner_t_impl = original_template_inner_type

// Can't just write "using SUPER::SUPER", because SUPER always uses ::crnt::r::DF,
// which means for different INSTANTIATION_TYPEs it isn't actually a base to the "s" struct.
// Use CSSH_##s and CURRENT_STRUCT_T_SUPER_HELPER_##s for this cases instead,
// as they are always the base class to the "s" regardless of what the INSTANTIATION_TYPE is.

#define CURRENT_USE_BASE_CONSTRUCTORS(s)                                                                             \
  using CRNT_super_t = ::crnt::r::SUPER_IMPL<CRH_FOR(s), INSTANTIATION_TYPE, CSSH_##s::INTERNAL_SUPER, std::false_type>; \
  using CRNT_super_t::CRNT_super_t

#define CURRENT_USE_T_BASE_CONSTRUCTORS(s)                                                                   \
  using CRNT_super_t =                                                                                       \
                         ::crnt::r::SUPER_IMPL<CRTH<s>,                                                      \
                                               INSTANTIATION_TYPE,                                           \
                                               typename CURRENT_STRUCT_T_SUPER_HELPER_##s::INTERNAL_SUPER_T, \
                                               std::true_type>;                                              \
  using CRNT_super_t::CRNT_super_t

#define CURRENT_EXTRACT_T_SUBTYPE_IMPL(subtype, exported_subtype)     \
  struct CRNT_##exported_subtype##_helper_t {                         \
    using subtype = int;                                              \
  };                                                                  \
  using exported_subtype =                                            \
    typename std::conditional_t<std::is_same_v<T, ::crnt::r::DummyT>, \
                                CRNT_##exported_subtype##_helper_t,   \
                                T>::subtype

#define CETS_IMPL1(a) CURRENT_EXTRACT_T_SUBTYPE_IMPL(a, a)
#define CETS_IMPL2(a, b) CURRENT_EXTRACT_T_SUBTYPE_IMPL(a, b)

#define CETS_N_ARGS_IMPL2(_1, _2, n, ...) n
#define CETS_N_ARGS_IMPL(args) CETS_N_ARGS_IMPL2 args

#define CETS_NARGS(...) CETS_N_ARGS_IMPL((__VA_ARGS__, 2, 1, 0))

#define CETS_CHOOSER2(n) CETS_IMPL##n
#define CETS_CHOOSER1(n) CETS_CHOOSER2(n)
#define CETS_CHOOSERX(n) CETS_CHOOSER1(n)

#define CETS_SWITCH(x, y) x y
#define CURRENT_EXTRACT_T_SUBTYPE(...) CETS_SWITCH(CETS_CHOOSERX(CETS_NARGS(__VA_ARGS__)), (__VA_ARGS__))

namespace current {

template <typename T>
constexpr bool HasPatchObjectImpl(char) {
  return false;
}

template <typename T>
constexpr auto HasPatchObjectImpl(int) -> decltype(sizeof(typename T::patch_object_t), bool()) {
  return true;
}

template <typename T>
constexpr bool HasPatch() { return HasPatchObjectImpl<T>(0); }

// TODO(dkorolev): Find a better place for this code. Must be included from the top-level `current.h`.

template <class B, class D>
struct is_same_or_base_of {
  constexpr static bool value = std::is_base_of_v<B, D>;
};
template <class C>
struct is_same_or_base_of<C, C> {
  constexpr static bool value = true;
};

}  // namespace current

namespace current {
namespace reflection {

// Required by Visual Studio 2015 Community. -- D.K.
template <typename INSTANTIATION_TYPE, typename T>
using Field = ::crnt::r::Field<INSTANTIATION_TYPE, T>;

using ::crnt::r::CurrentStructFieldsConsistency;
using ::crnt::r::FieldType;
using ::crnt::r::FieldTypeWrapper;

}  // namespace current::reflection
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_STRUCT_H
