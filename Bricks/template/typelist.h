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

// TODO(dkorolev): Migrate other usecases of `std::tuple<>` to support `TypeList<>`.
// TODO(dkorolev): Eventually move all `static_assert()`-s into the test for faster compilation.

#ifndef BRICKS_TEMPLATE_TYPELIST_H
#define BRICKS_TEMPLATE_TYPELIST_H

#include <cstdlib>
#include <type_traits>

namespace current {
namespace metaprogramming {

// `TypeListImpl<...>` is the underlying class for type list.
// This type is `returned` by `TypeList<...>`, as the latter `call` does flattening and deduplication.
template <typename... TS>
struct TypeListImpl {
  constexpr static size_t size = sizeof...(TS);
};

// `TypeListSizeExtractor<>` makes sure the top-level `TypeListSize<>` below
// can only be invoked on `TypeListImpl<>`-s.
template <typename T>
struct TypeListSizeExtractor {};

template <typename... TS>
struct TypeListSizeExtractor<TypeListImpl<TS...>> {
  constexpr static size_t value = TypeListImpl<TS...>::size;
};

// `TypeListSize<TypeListImpl<TS...>>::value` is the number of types in `TS...`.
template <typename T>
using TypeListSize = TypeListSizeExtractor<T>;

static_assert(TypeListSize<TypeListImpl<>>::value == 0, "");
static_assert(TypeListSize<TypeListImpl<int>>::value == 1, "");
static_assert(TypeListSize<TypeListImpl<int, int>>::value == 2, "");
static_assert(TypeListSize<TypeListImpl<int, int, int>>::value == 3, "");

// `DuplicateTypeInTypeList` and `ConfirmTypeListContainsNoDuplicates` are just compile-error-readable names
// for two helper classes that wrap contained types and inherit from all the wrapped types respectively.
// The trick used in `TypeListContains` and `TypeListCat` is that a type can't be listed twice as a base class.
template <typename T>
struct DuplicateTypeInTypeList {};

template <typename... TS>
struct ConfirmTypeListContainsNoDuplicates : DuplicateTypeInTypeList<TS>... {};

// `TypeListContains<TypeListImpl<TS...>, T>::value` is true iff `T` is contained in `TS...`.`
template <typename TYPE_LIST_IMPL, typename TYPE>
struct TypeListContains;

template <typename... TS, typename T>
struct TypeListContains<TypeListImpl<TS...>, T> {
  enum { value = std::is_base_of<DuplicateTypeInTypeList<T>, ConfirmTypeListContainsNoDuplicates<TS...>>::value };
};

static_assert(!TypeListContains<TypeListImpl<>, int>::value, "");
static_assert(TypeListContains<TypeListImpl<int>, int>::value, "");
static_assert(!TypeListContains<TypeListImpl<int>, TypeListImpl<int>>::value, "");

static_assert(!TypeListContains<TypeListImpl<int, double>, char>::value, "");

static_assert(TypeListContains<TypeListImpl<char, int, double>, char>::value, "");
static_assert(TypeListContains<TypeListImpl<int, char, double>, char>::value, "");
static_assert(TypeListContains<TypeListImpl<int, double, char>, char>::value, "");

static_assert(TypeListContains<TypeListImpl<char, int>, char>::value, "");
static_assert(TypeListContains<TypeListImpl<int, char>, char>::value, "");

// `TypeListCat` creates a `TypeList<LHS..., RHS...>` given `TypeList<LHS...>` and `TypeList<RHS...>`.
// By design, it would fail at compile tim if `LHS...` and `RHS...` have at least one shared type.
template <typename LHS, typename RHS>
struct TypeListCatImpl;

template <typename... LHS, typename... RHS>
struct TypeListCatImpl<TypeListImpl<LHS...>, TypeListImpl<RHS...>> : DuplicateTypeInTypeList<LHS>...,
                                                                     DuplicateTypeInTypeList<RHS>... {
  using result = TypeListImpl<LHS..., RHS...>;
};

template <typename LHS_TYPELIST, typename RHS_TYPELIST>
struct TypeListCatWindowsWrapper;

template <typename... LHS, typename... RHS>
struct TypeListCatWindowsWrapper<TypeListImpl<LHS...>, TypeListImpl<RHS...>> {
  using result = typename TypeListCatImpl<TypeListImpl<LHS...>, TypeListImpl<RHS...>>::result;
};

template <typename LHS_TYPELIST, typename RHS_TYPELIST>
using TypeListCat = typename TypeListCatWindowsWrapper<LHS_TYPELIST, RHS_TYPELIST>::result;

static_assert(std::is_same<TypeListImpl<>, TypeListCat<TypeListImpl<>, TypeListImpl<>>>::value, "");
static_assert(std::is_same<TypeListImpl<int>, TypeListCat<TypeListImpl<int>, TypeListImpl<>>>::value, "");
static_assert(std::is_same<TypeListImpl<int>, TypeListCat<TypeListImpl<>, TypeListImpl<int>>>::value, "");
static_assert(
    std::is_same<TypeListImpl<int, char, double>, TypeListCat<TypeListImpl<int>, TypeListImpl<char, double>>>::value,
    "");
static_assert(
    std::is_same<TypeListImpl<int, char, double>, TypeListCat<TypeListImpl<int, char>, TypeListImpl<double>>>::value,
    "");

// `TypeListUnion` creates a `TypeList<{common part of LHS.. and RHS...}>`. It's a `TypeListCat` that allows duplicates.
template <typename RESULT, typename LHS, typename RHS>
struct TypeListUnionImpl;

template <typename RESULTING_TYPELIST, typename T, bool CONTAINS, typename SLACK1, typename SLACK2>
struct TypeListUnionImplAppendIfNotPresent;

template <typename RESULTING_TYPELIST, typename T, typename SLACK1, typename SLACK2>
struct TypeListUnionImplAppendIfNotPresent<RESULTING_TYPELIST, T, true, SLACK1, SLACK2> {
  using result = typename TypeListUnionImpl<RESULTING_TYPELIST, SLACK1, SLACK2>::result;
};

template <typename... RESULTING_TYPES, typename T, typename SLACK1, typename SLACK2>
struct TypeListUnionImplAppendIfNotPresent<TypeListImpl<RESULTING_TYPES...>, T, false, SLACK1, SLACK2> {
  using result = typename TypeListUnionImpl<TypeListImpl<RESULTING_TYPES..., T>, SLACK1, SLACK2>::result;
};

template <typename RESULTING_TYPELIST, typename RHS_TYPELIST, typename T, typename... TS>
struct TypeListUnionImpl<RESULTING_TYPELIST, TypeListImpl<T, TS...>, RHS_TYPELIST> {
  using result = typename TypeListUnionImplAppendIfNotPresent<RESULTING_TYPELIST,
                                                              T,
                                                              TypeListContains<RESULTING_TYPELIST, T>::value,
                                                              TypeListImpl<TS...>,
                                                              RHS_TYPELIST>::result;
};

template <typename RESULTING_TYPELIST, typename T, typename... TS>
struct TypeListUnionImpl<RESULTING_TYPELIST, TypeListImpl<>, TypeListImpl<T, TS...>> {
  using result = typename TypeListUnionImplAppendIfNotPresent<RESULTING_TYPELIST,
                                                              T,
                                                              TypeListContains<RESULTING_TYPELIST, T>::value,
                                                              TypeListImpl<>,
                                                              TypeListImpl<TS...>>::result;
};

template <typename RESULTING_TYPELIST>
struct TypeListUnionImpl<RESULTING_TYPELIST, TypeListImpl<>, TypeListImpl<>> {
  using result = RESULTING_TYPELIST;
};

template <typename LHS_TYPELIST, typename RHS_TYPELIST>
struct TypeListUnionWindowsWrapper;

template <typename... LHS, typename... RHS>
struct TypeListUnionWindowsWrapper<TypeListImpl<LHS...>, TypeListImpl<RHS...>> {
  using result = typename TypeListUnionImpl<TypeListImpl<>, TypeListImpl<LHS...>, TypeListImpl<RHS...>>::result;
};

template <typename LHS_TYPELIST, typename RHS_TYPELIST>
using TypeListUnion = typename TypeListUnionWindowsWrapper<LHS_TYPELIST, RHS_TYPELIST>::result;

static_assert(std::is_same<TypeListImpl<>, TypeListUnion<TypeListImpl<>, TypeListImpl<>>>::value, "");
static_assert(std::is_same<TypeListImpl<int>, TypeListUnion<TypeListImpl<int>, TypeListImpl<>>>::value, "");
static_assert(std::is_same<TypeListImpl<int>, TypeListUnion<TypeListImpl<>, TypeListImpl<int>>>::value, "");
static_assert(std::is_same<TypeListImpl<int>, TypeListUnion<TypeListImpl<int>, TypeListImpl<int>>>::value, "");
static_assert(std::is_same<TypeListImpl<int, char, double>,
                           TypeListUnion<TypeListImpl<int, char>, TypeListImpl<char, double>>>::value,
              "");

// `FlattenImpl<TypeListImpl<LHS...>, TypeListImpl<REST...>>` flattens all the types from `REST...`
// and appends them to `TypeListImpl<LHS...>`.
// It does not do type deduplication.
template <typename LHS, typename REST>
struct FlattenImpl {};

template <typename... LHS>
struct FlattenImpl<TypeListImpl<LHS...>, TypeListImpl<>> {
  using flattened = TypeListImpl<LHS...>;
};

template <typename... LHS, typename... NESTED, typename... REST>
struct FlattenImpl<TypeListImpl<LHS...>, TypeListImpl<TypeListImpl<NESTED...>, REST...>> {
  using flattened = typename FlattenImpl<TypeListImpl<LHS...>, TypeListImpl<NESTED..., REST...>>::flattened;
};

template <typename... LHS, typename T, typename... REST>
struct FlattenImpl<TypeListImpl<LHS...>, TypeListImpl<T, REST...>> {
  using flattened = typename FlattenImpl<TypeListImpl<LHS..., T>, TypeListImpl<REST...>>::flattened;
};

// `FlattenImplExtractor<>` makes sure the top-level `Flatten<>` below
// can only be invoked on `TypeListImpl<>`-s.
template <typename T>
struct FlattenImplExtractor {};

template <typename... TS>
struct FlattenImplExtractor<TypeListImpl<TS...>> {
  using extracted_flattened = typename FlattenImpl<TypeListImpl<>, TypeListImpl<TS...>>::flattened;
};

// User-facing `Flatten<TypeListImpl<TS...>>` recursively flattens all `TypeListImpl<>`-s within `TS...`.
template <typename INPUT_TYPELIST>
using Flatten = typename FlattenImplExtractor<INPUT_TYPELIST>::extracted_flattened;

static_assert(std::is_same<TypeListImpl<>, Flatten<TypeListImpl<>>>::value, "");
static_assert(std::is_same<TypeListImpl<int>, Flatten<TypeListImpl<int>>>::value, "");
static_assert(std::is_same<TypeListImpl<int>, Flatten<TypeListImpl<TypeListImpl<int>>>>::value, "");
static_assert(std::is_same<TypeListImpl<int>, Flatten<TypeListImpl<TypeListImpl<TypeListImpl<int>>>>>::value, "");
static_assert(std::is_same<TypeListImpl<int, double>, Flatten<TypeListImpl<int, double>>>::value, "");
static_assert(std::is_same<TypeListImpl<int, double>, Flatten<TypeListImpl<TypeListImpl<int>, double>>>::value, "");
static_assert(
    std::is_same<TypeListImpl<int, double>, Flatten<TypeListImpl<TypeListImpl<TypeListImpl<int>>, double>>>::value, "");

// Performance alert, @dkorolev @mzhurovich 12/5/2015. Revisited and kept the semantics same. -- D.K. 10/15/2015.
// The implementation of `TypeList` deduplication+flattening is very inefficient as of now.
// We are making the default behavior of `TypeList` a non-deduplicating non-flattening one, and introducing
// a special syntax of `SlowTypeList` for the case where deduplication and/or flattening is indeed requred.

// User-facing `SlowTypeList<TS...>` creates a type list that contains the union of all passed in types,
// performing flattening and deduplication of all input types.
template <typename... TS>
using SlowTypeList = TypeListUnion<TypeListImpl<>, Flatten<TypeListImpl<TS...>>>;

// User-facing `TypeList<TS...>` assumes `TS...` are unique and not-nested.
template <typename... TS>
using TypeList = TypeListImpl<TS...>;

static_assert(std::is_same<TypeListImpl<>, SlowTypeList<>>::value, "");
static_assert(std::is_same<TypeListImpl<int>, SlowTypeList<int>>::value, "");
static_assert(std::is_same<TypeListImpl<int>, SlowTypeList<SlowTypeList<SlowTypeList<int>>>>::value, "");
static_assert(std::is_same<TypeListImpl<int>, SlowTypeList<int, int>>::value, "");
static_assert(std::is_same<TypeListImpl<int>, SlowTypeList<int, SlowTypeList<int>>>::value, "");
static_assert(std::is_same<TypeListImpl<int>, SlowTypeList<int, SlowTypeList<SlowTypeList<int>>>>::value, "");
static_assert(
    std::is_same<TypeListImpl<int, double>, SlowTypeList<int, SlowTypeList<SlowTypeList<int>, double>>>::value, "");
static_assert(std::is_same<TypeListImpl<int, double>, SlowTypeList<int, int, double, double>>::value, "");
static_assert(std::is_same<TypeListImpl<int, double>, SlowTypeList<int, double, int>>::value, "");
static_assert(std::is_same<TypeListImpl<int, double>, SlowTypeList<int, double, int, double>>::value, "");
static_assert(std::is_same<TypeListImpl<int, double>, SlowTypeList<SlowTypeList<int, double, int, double>>>::value, "");

static_assert(!std::is_same<TypeListImpl<int>, SlowTypeList<double>>::value, "");
static_assert(!std::is_same<TypeListImpl<int, double>, SlowTypeList<double, int>>::value, "");

template <typename... TS>
struct IsTypeList {
  enum { value = false };
};

template <typename... TS>
struct IsTypeList<TypeListImpl<TS...>> {
  enum { value = true };
};

static_assert(IsTypeList<TypeListImpl<>>::value, "");
static_assert(IsTypeList<TypeListImpl<int>>::value, "");
static_assert(IsTypeList<TypeListImpl<int, double>>::value, "");
static_assert(!IsTypeList<int>::value, "");
static_assert(!IsTypeList<double>::value, "");

// `EvensOnly<TypeListImpl<...>>` extracts only the types at even 1-based indexes: first, third, etc.
template <typename, typename>
struct EvensOnlyImpl;

template <typename EVENS_TYPELIST>
struct EvensOnlyImpl<EVENS_TYPELIST, TypeListImpl<>> {
  using result = EVENS_TYPELIST;
};

template <typename... EVENS, typename GOOD>
struct EvensOnlyImpl<TypeListImpl<EVENS...>, TypeListImpl<GOOD>> {
  using result = TypeListImpl<EVENS..., GOOD>;
};

template <typename... EVENS, typename GOOD, typename BAD, typename... UGLY>
struct EvensOnlyImpl<TypeListImpl<EVENS...>, TypeListImpl<GOOD, BAD, UGLY...>> {
  using result = typename EvensOnlyImpl<TypeListImpl<EVENS..., GOOD>, TypeListImpl<UGLY...>>::result;
};

template <typename T>
using EvensOnly = typename EvensOnlyImpl<TypeListImpl<>, T>::result;

}  // namespace metaprogramming
}  // namespace current

namespace crnt {
namespace tle {

using ::current::metaprogramming::TypeListImpl;

// `TypeIndex`.
template <size_t>
struct TI {};

// `TypeWrapper`.
template <typename T>
struct TW {
  using type = T;
};

// `EnumerateTypesInTypeList`.
template <size_t I, typename TYPE_LIST_IMPL>
struct E;

template <size_t I>
struct E<I, TypeListImpl<>> {
  // ``TypeAtIndex`.
  void F(TI<I>) {}
};

template <size_t I, typename T, typename... TS>
struct E<I, TypeListImpl<T, TS...>> : E<I + 1u, TypeListImpl<TS...>> {
  using E<I + 1u, TypeListImpl<TS...>>::F;
  TW<T> F(TI<I>) { return TW<T>(); }
};

// `TypeListEnumerator`.
template <typename... TS>
struct TLE : E<0, TS...> {};

// `TypeListElementImpl`.
template <size_t N, typename TYPE_LIST_IMPL>
struct Q {
  using type_wrapper = decltype((std::declval<TLE<TYPE_LIST_IMPL>>()).F(TI<N>()));
  using type = typename type_wrapper::type;
};

}  // namespace crnt::tle
}  // namespace crnt

namespace current {
namespace metaprogramming {

// `TypeListElement<N, TypeListImpl<TS....>>` evaluates to the N-th (0-based) type of `TS...`.
template <size_t N, typename TYPE_LIST_IMPL>
using TypeListElement = typename ::crnt::tle::Q<N, TYPE_LIST_IMPL>::type;

static_assert(std::is_same<int, TypeListElement<0, TypeListImpl<int>>>::value, "");
static_assert(std::is_same<long, TypeListElement<0, TypeListImpl<long, char>>>::value, "");
static_assert(std::is_same<char, TypeListElement<1, TypeListImpl<long, char>>>::value, "");

}  // namespace metaprogramming
}  // namespace current

// Export some symbols into global scope.
using current::metaprogramming::TypeList;
using current::metaprogramming::SlowTypeList;
using current::metaprogramming::TypeListContains;
using current::metaprogramming::IsTypeList;
using current::metaprogramming::TypeListSize;
using current::metaprogramming::TypeListElement;

// Note: For equality and lack of discrimination reasons, the user may still use raw `TypeListImpl`,
// if she prefers to not have flattening and deduplication take place.
using current::metaprogramming::TypeListImpl;

#endif  // BRICKS_TEMPLATE_TYPELIST_H
