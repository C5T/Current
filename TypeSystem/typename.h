/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

// Get Current-specific type name. Supports `CURRENT_STRUCT`-s and `CURRENT_VARIANT`-s. Used for schema export.

#ifndef CURRENT_TYPE_SYSTEM_TYPENAME_H
#define CURRENT_TYPE_SYSTEM_TYPENAME_H

#include "../port.h"

#include "types.h"

#include "../Bricks/template/decay.h"

namespace current {
namespace reflection {

template <typename T, bool TRUE_IF_CURRENT_STRUCT, bool TRUE_IF_VARIANT>
struct CurrentTypeNameImpl;

template <typename T>
struct CurrentTypeNameImpl<T, true, false> {
  static const char* GetCurrentTypeName() { return T::CURRENT_STRUCT_NAME(); }
};

template <typename T>
struct CurrentTypeNameImpl<T, false, true> {
  static std::string GetCurrentTypeName() { return T::VariantName(); }
};

template <typename T>
inline std::string CurrentTypeName() {
  return CurrentTypeNameImpl<T, IS_CURRENT_STRUCT(T), IS_CURRENT_VARIANT(T)>::GetCurrentTypeName();
}

// For JSON serialization and other repetitive operations, enable returning type name as a `const char*`.
template <typename T, typename>
struct CurrentTypeNameAsConstCharPtrImpl {
  static const char* Value() {
    static const std::string cached_name = CurrentTypeName<T>();
    return cached_name.c_str();
  }
};

template <typename T>
struct CurrentTypeNameAsConstCharPtrImpl<T, const char*> {
  static const char* Value() { return CurrentTypeName<T>(); }
};

template <typename T>
inline const char* CurrentTypeNameAsConstCharPtr() {
  using impl_t = CurrentTypeNameAsConstCharPtrImpl<T, decltype(CurrentTypeName<T>())>;
  return impl_t::Value();
}

namespace impl {

template <typename T>
struct TypeListNamesAsCSV;

template <typename T>
struct TypeListNamesAsCSV<TypeListImpl<T>> {
  static std::string Value() { return CurrentTypeName<T>(); }
};

template <typename T, typename... TS>
struct TypeListNamesAsCSV<TypeListImpl<T, TS...>> {
  static std::string Value() {
    return CurrentTypeName<T>() + '_' + TypeListNamesAsCSV<TypeListImpl<TS...>>::Value();
  }
};

template <typename T>
struct VariantFullNamePrinter {
  static std::string Value() { return "Variant_B_" + TypeListNamesAsCSV<T>::Value() + "_E"; }
};

}  // namespace current::reflection::impl

struct CurrentVariantDefaultName final {
  template <typename TYPELIST>
  static std::string VariantNameImpl() {
    return impl::VariantFullNamePrinter<TYPELIST>::Value();
  }
};

}  // namespace current::reflection
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_TYPENAME_H
