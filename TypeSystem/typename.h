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

#include <chrono>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "types.h"
#include "optional.h"
#include "enum.h"

#include "../Bricks/template/decay.h"

namespace current {
namespace reflection {

namespace i {  // impl

template <typename T, bool TRUE_IF_CURRENT_STRUCT, bool TRUE_IF_VARIANT, bool TRUE_IF_ENUM>
struct CurrentTypeNameImpl;

template <typename T>
struct CurrentTypeNameCaller {
  using impl_t = CurrentTypeNameImpl<T, IS_CURRENT_STRUCT(T), IS_CURRENT_VARIANT(T), std::is_enum<T>::value>;
  static const char* CallGetCurrentTypeName() {
    static const std::string value = impl_t::GetCurrentTypeName();
    return value.c_str();
  }
};

template <typename T, bool>
struct CallRightGetCurrentStructName {
  static const char* DoIt() { return T::CURRENT_STRUCT_NAME(); }
};

template <typename T>
struct CallRightGetCurrentStructName<T, true> {
  static const char* DoIt() { return T::CURRENT_EXPORTED_STRUCT_NAME(); }
};

template <typename T>
struct CurrentTypeNameImpl<T, true, false, false> {
  static const char* GetCurrentTypeName() {
    return CallRightGetCurrentStructName<T, sfinae::Has_CURRENT_EXPORTED_STRUCT_NAME<T>::value>::DoIt();
  }
};

template <typename T>
struct CurrentTypeNameImpl<T, false, true, false> {
  static std::string GetCurrentTypeName() { return T::VariantName(); }
};

template <typename T>
struct CurrentTypeNameImpl<T, false, false, true> {
  static std::string GetCurrentTypeName() { return reflection::EnumName<T>(); }
};

#define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, cpp_type, current_type, fs_type, md_type, typescript_type) \
  template <>                                                                                                   \
  struct CurrentTypeNameImpl<cpp_type, false, false, false> {                                                   \
    static const char* GetCurrentTypeName() { return #cpp_type; }                                               \
  };
#include "primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

template <typename T>
struct CurrentTypeNameImpl<std::vector<T>, false, false, false> {
  static std::string GetCurrentTypeName() {
    return std::string("std::vector<") + CurrentTypeNameCaller<T>::CallGetCurrentTypeName() + '>';
  }
};

template <typename T>
struct CurrentTypeNameImpl<std::set<T>, false, false, false> {
  static std::string GetCurrentTypeName() {
    return std::string("std::set<") + CurrentTypeNameCaller<T>::CallGetCurrentTypeName() + '>';
  }
};

template <typename T>
struct CurrentTypeNameImpl<std::unordered_set<T>, false, false, false> {
  static std::string GetCurrentTypeName() {
    return std::string("std::unordered_set<") + CurrentTypeNameCaller<T>::CallGetCurrentTypeName() + '>';
  }
};

template <typename TF, typename TS>
struct CurrentTypeNameImpl<std::pair<TF, TS>, false, false, false> {
  static std::string GetCurrentTypeName() {
    return std::string("std::pair<") + CurrentTypeNameCaller<TF>::CallGetCurrentTypeName() + ", " +
           CurrentTypeNameCaller<TS>::CallGetCurrentTypeName() + '>';
  }
};

template <typename TK, typename TV>
struct CurrentTypeNameImpl<std::map<TK, TV>, false, false, false> {
  static std::string GetCurrentTypeName() {
    return std::string("std::map<") + CurrentTypeNameCaller<TK>::CallGetCurrentTypeName() + ", " +
           CurrentTypeNameCaller<TV>::CallGetCurrentTypeName() + '>';
  }
};

template <typename TK, typename TV>
struct CurrentTypeNameImpl<std::unordered_map<TK, TV>, false, false, false> {
  static std::string GetCurrentTypeName() {
    return std::string("std::unordered_map<") + CurrentTypeNameCaller<TK>::CallGetCurrentTypeName() + ", " +
           CurrentTypeNameCaller<TV>::CallGetCurrentTypeName() + '>';
  }
};

template <typename T>
struct CurrentTypeNameImpl<Optional<T>, false, false, false> {
  static std::string GetCurrentTypeName() {
    return std::string("Optional<") + CurrentTypeNameCaller<T>::CallGetCurrentTypeName() + '>';
  }
};

}  // namespace current::reflection::i

template <typename T>
inline const char* CurrentTypeName() {
  return i::CurrentTypeNameCaller<T>::CallGetCurrentTypeName();
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
    return std::string(CurrentTypeName<T>()) + '_' + TypeListNamesAsCSV<TypeListImpl<TS...>>::Value();
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
