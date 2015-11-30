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

#ifndef CURRENT_TYPE_SYSTEM_SFINAE_H
#define CURRENT_TYPE_SYSTEM_SFINAE_H

#include <map>
#include <utility>

#include "../Bricks/template/pod.h"
#include "../Bricks/template/enable_if.h"

namespace current {
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

// Whether a `ValueImpl()` method is defined for a type.
template <typename ENTRY>
constexpr bool HasValueImplMethod(char) {
  return false;
}

template <typename ENTRY>
constexpr auto HasValueImplMethod(int) -> decltype(std::declval<const ENTRY>().ValueImpl(), bool()) {
  return true;
}

// Whether an `CheckIntegrityImpl()` method is defined for a type.
template <typename ENTRY>
constexpr bool HasCheckIntegrityImplMethod(char) {
  return false;
}

template <typename ENTRY>
constexpr auto HasCheckIntegrityImplMethod(int)
    -> decltype(std::declval<const ENTRY>().CheckIntegrityImpl(), bool()) {
  return true;
}

// TODO(dkorolev): `unordered_*` as well, for stripping and unstripping.

// Unstripped types, via `T::UNSTRIPPED_TYPE` or natively.
template <typename T>
constexpr bool HasUnstrippedType(char) {
  return false;
}

template <typename T>
constexpr auto HasUnstrippedType(int) -> decltype(sizeof(typename T::UNSTRIPPED_TYPE), bool()) {
  return true;
}

template <typename T, bool>
struct UnstrippedTypeImpl {
  typedef T type;
};

template <typename T>
using Unstripped = typename sfinae::UnstrippedTypeImpl<T, sfinae::HasUnstrippedType<T>(0)>::type;

template <typename TF, typename TS>
struct UnstrippedTypeImpl<std::pair<TF, TS>, false> {
  typedef std::pair<Unstripped<TF>, Unstripped<TS>> type;
};

template <typename TK, typename TV, typename CMP>
struct UnstrippedTypeImpl<std::map<TK, TV, CMP>, false> {
  typedef std::map<Unstripped<TK>, Unstripped<TV>> type;
};

template <typename T>
struct UnstrippedTypeImpl<T, true> {
  typedef typename T::UNSTRIPPED_TYPE type;
};

// Stripped types, via `T::STRIPPED_TYPE` or natively.
template <typename T>
constexpr bool HasStrippedType(char) {
  return false;
}

template <typename T>
constexpr auto HasStrippedType(int) -> decltype(sizeof(typename T::STRIPPED_TYPE), bool()) {
  return true;
}

template <typename T, bool>
struct StrippedTypeImpl {
  typedef T type;
};

template <typename T>
using Stripped = typename sfinae::StrippedTypeImpl<T, sfinae::HasStrippedType<T>(0)>::type;

template <typename TF, typename TS>
struct StrippedTypeImpl<std::pair<TF, TS>, false> {
  typedef std::pair<Stripped<TF>, Stripped<TS>> type;
};

template <typename T>
struct ComparatorForStrippedType {
  bool operator()(current::copy_free<Stripped<T>> lhs, current::copy_free<Stripped<T>> rhs) const {
    return (*reinterpret_cast<const Unstripped<T>*>(&lhs)) < (*reinterpret_cast<const Unstripped<T>*>(&rhs));
  }
};

template <typename TK, typename TV>
struct StrippedTypeImpl<std::map<TK, TV>, false> {
  typedef std::map<Stripped<TK>, Stripped<TV>, ComparatorForStrippedType<TK>> type;
};

template <typename T>
struct StrippedTypeImpl<T, true> {
  typedef typename T::STRIPPED_TYPE type;
};

template <typename T>
T&& MoveFromStripped(Stripped<T>&& input) {
  // `reinterpret_cast<>` does it since the types are the same down to
  // the compile-time check of whether the default constructor is explicitly disabled.
  static_assert(sizeof(T) == sizeof(Stripped<T>), "");
  return std::move(*reinterpret_cast<T*>(&input));
}

}  // namespace sfinae

using sfinae::Stripped;
using sfinae::Unstripped;
using sfinae::MoveFromStripped;

}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SFINAE_H
