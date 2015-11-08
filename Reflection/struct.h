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

#ifndef CURRENT_REFLECTION_STRUCT_H
#define CURRENT_REFLECTION_STRUCT_H

#include <memory>
#include <type_traits>

#include "base.h"

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

}  // namespace reflection
}  // namespace current

#define CURRENT_EXPAND_MACRO_IMPL(x) x
#define CURRENT_EXPAND_MACRO(x) CURRENT_EXPAND_MACRO_IMPL(x)

// Macros for structure definition.
#define CURRENT_STRUCT_SWITCH(_1, _2, F, ...) F
#define CURRENT_STRUCT(...) \
  CURRENT_STRUCT_SWITCH(__VA_ARGS__, CURRENT_STRUCT_DERIVED, CURRENT_STRUCT_NOT_DERIVED)(__VA_ARGS__)

#define CURRENT_STRUCT_HELPERS(s, super)                                                    \
  template <typename INSTANTIATION_TYPE>                                                    \
  struct CURRENT_STRUCT_IMPL_##s;                                                           \
  using s = CURRENT_STRUCT_IMPL_##s<::current::reflection::DeclareFields>;                  \
  template <typename T>                                                                     \
  struct CURRENT_REFLECTION_HELPER;                                                         \
  template <>                                                                               \
  struct CURRENT_REFLECTION_HELPER<s> {                                                     \
    typedef super SUPER;                                                                    \
    constexpr static const char* name() { return #s; }                                      \
    constexpr static size_t index_base = __COUNTER__;                                       \
    typedef CURRENT_STRUCT_IMPL_##s<::current::reflection::CountFields> field_count_struct; \
  }

#define CURRENT_STRUCT_NOT_DERIVED(s)                             \
  CURRENT_STRUCT_HELPERS(s, ::current::reflection::CurrentSuper); \
  template <typename INSTANTIATION_TYPE>                          \
  struct CURRENT_STRUCT_IMPL_##s                                  \
      : CURRENT_REFLECTION_HELPER<s>,                             \
        ::current::reflection::BaseTypeHelper<INSTANTIATION_TYPE, ::current::reflection::CurrentSuper>

#define CURRENT_STRUCT_DERIVED(s, base)                                            \
  static_assert(std::is_base_of<::current::reflection::CurrentSuper, base>::value, \
                #base " must be derived from `CurrentSuper`.");                    \
  CURRENT_STRUCT_HELPERS(s, base);                                                 \
  template <typename INSTANTIATION_TYPE>                                           \
  struct CURRENT_STRUCT_IMPL_##s : CURRENT_REFLECTION_HELPER<s>,                   \
                                   ::current::reflection::BaseTypeHelper<INSTANTIATION_TYPE, base>

#define CURRENT_FIELD_SWITCH(_1, _2, _3, F, ...) F
#define CURRENT_FIELD(...) \
  CURRENT_FIELD_SWITCH(__VA_ARGS__, CURRENT_FIELD_WITH_VALUE, CURRENT_FIELD_WITH_NO_VALUE)(__VA_ARGS__)

#define CURRENT_FIELD_WITH_NO_VALUE(name, type)                \
  ::current::reflection::Field<INSTANTIATION_TYPE, type> name; \
  CURRENT_FIELD_REFLECTION(CURRENT_EXPAND_MACRO(__COUNTER__) - index_base - 1, type, name)

#define CURRENT_FIELD_WITH_VALUE(name, type, value)                   \
  ::current::reflection::Field<INSTANTIATION_TYPE, type> name{value}; \
  CURRENT_FIELD_REFLECTION(CURRENT_EXPAND_MACRO(__COUNTER__) - index_base - 1, type, name)

#define CURRENT_FIELD_REFLECTION(idx, type, name)                                                              \
  template <class F>                                                                                           \
  static void CURRENT_REFLECTION(F&& f,                                                                        \
                                 ::current::reflection::Index<::current::reflection::FieldTypeAndName, idx>) { \
    f(::current::reflection::TypeSelector<type>(), #name);                                                     \
  }                                                                                                            \
  template <class F>                                                                                           \
  void CURRENT_REFLECTION(                                                                                     \
      F&& f, ::current::reflection::Index<::current::reflection::FieldNameAndImmutableValue, idx>) const {     \
    f(#name, name);                                                                                            \
  }                                                                                                            \
  template <class F>                                                                                           \
  void CURRENT_REFLECTION(                                                                                     \
      F&& f, ::current::reflection::Index<::current::reflection::FieldNameAndMutableValue, idx>) {             \
    f(#name, name);                                                                                            \
  }

#define RETURNS(RETURN_TYPE)                                                                                  \
  template <typename INSTANTIATION_TYPE_IMPL = INSTANTIATION_TYPE>                                            \
  typename std::enable_if<std::is_same<INSTANTIATION_TYPE_IMPL, ::current::reflection::DeclareFields>::value, \
                          RETURN_TYPE>::type

#define CURRENT_CONSTRUCTOR(s)                                                                             \
  template <typename INSTANTIATION_TYPE_IMPL = INSTANTIATION_TYPE,                                         \
            class = typename std::enable_if<                                                               \
                std::is_same<INSTANTIATION_TYPE_IMPL, ::current::reflection::DeclareFields>::value>::type> \
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
  static_assert(std::is_base_of<CurrentSuper, T>::value,
                "`SuperType` must be called with the type defined via `CURRENT_STRUCT` macro.");
  typedef typename T::template CURRENT_REFLECTION_HELPER<T>::SUPER type;
};

template <typename T>
using SuperType = typename SuperTypeImpl<T>::type;

template <typename T>
struct FieldCounter {
  static_assert(std::is_base_of<CurrentSuper, T>::value,
                "`FieldCounter` must be called with the type defined via `CURRENT_STRUCT` macro.");
  enum {
    value = (sizeof(typename T::template CURRENT_REFLECTION_HELPER<T>::field_count_struct) /
             sizeof(CountFieldsImplementationType))
  };
};

template <typename T, typename VISITOR_TYPE>
struct VisitAllFields {
  static_assert(std::is_base_of<CurrentSuper, T>::value,
                "`VisitAllFields` must be called with the type defined via `CURRENT_STRUCT` macro.");
  typedef bricks::variadic_indexes::generate_indexes<FieldCounter<T>::value> NUM_INDEXES;

  // Visit all fields without an object. Used for enumerating fields and generating signatures.
  template <typename F>
  static void WithoutObject(const F& f) {
    static NUM_INDEXES all_indexes;
    WithoutObjectImpl(f, all_indexes);
  }

  // Visit all fields with an object, const or mutable. Used for serialization.
  template <typename TT, typename F>
  static void WithObject(TT&& t, const F& f) {
    static NUM_INDEXES all_indexes;
    WithObjectImpl(std::forward<TT>(t), f, all_indexes);
  }

 private:
  template <typename F, int N, int... NS>
  static void WithoutObjectImpl(const F& f, bricks::variadic_indexes::indexes<N, NS...>) {
    static bricks::variadic_indexes::indexes<NS...> remaining_indexes;
    T::CURRENT_REFLECTION(f, Index<VISITOR_TYPE, N>());
    WithoutObjectImpl(f, remaining_indexes);
  }

  template <typename F>
  static void WithoutObjectImpl(const F&, bricks::variadic_indexes::indexes<>) {}

  template <typename TT, typename F, int N, int... NS>
  static void WithObjectImpl(TT&& t, const F& f, bricks::variadic_indexes::indexes<N, NS...>) {
    static bricks::variadic_indexes::indexes<NS...> remaining_indexes;
    t.CURRENT_REFLECTION(f, Index<VISITOR_TYPE, N>());
    WithObjectImpl(std::forward<TT>(t), f, remaining_indexes);
  }

  template <typename TT, typename F>
  static void WithObjectImpl(TT&&, const F&, bricks::variadic_indexes::indexes<>) {}
};

}  // namespace reflection
}  // namespace current

#endif  // CURRENT_REFLECTION_STRUCT_H
