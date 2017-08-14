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

template <typename NAME, typename TYPE_LIST>
struct VariantImpl;

namespace reflection {

struct CurrentVariantDefaultName;

// `DebugDump`: ex. "Templated<std::vector<std::string>>", "std::vector<Templated<uint64_t>>". C++-level output.
// `Z`: ex. "std::vector<std::string>", "Templated_Z". *ONLY* for legacy reasons of TypeID computation, TO BE FIXED.
// `AsIdentifier`: ex. "Vector_Templated_T_Foo", for serialization and for schema dumps into {Java,Type}Script et. al.
enum class NameFormat : int { DebugDump = 0, Z, AsIdentifier };

namespace impl {

template <NameFormat NF, typename T, bool TRUE_IF_CURRENT_STRUCT, bool TRUE_IF_VARIANT, bool TRUE_IF_ENUM>
struct CurrentTypeNameImpl;

template <NameFormat NF, typename T>
struct CurrentTypeNameCaller {
  using impl_t = CurrentTypeNameImpl<NF, T, IS_CURRENT_STRUCT(T), IS_CURRENT_VARIANT(T), std::is_enum<T>::value>;
  static const char* CallGetCurrentTypeName() {
    static const std::string value = impl_t::GetCurrentTypeName();
    return value.c_str();
  }
};

template <NameFormat NF, typename T>
struct JoinTypeNames;

template <typename T, bool>
struct CallRightGetCurrentStructName {
  static const char* DoIt() { return T::CURRENT_STRUCT_NAME(); }
};

template <typename T>
struct CallRightGetCurrentStructName<T, true> {
  static const char* DoIt() { return T::CURRENT_EXPORTED_STRUCT_NAME(); }
};

template <NameFormat NF, typename T>
struct CurrentTypeNameImpl<NF, T, true, false, false> {
  static const char* GetCurrentTypeName() {
    return CallRightGetCurrentStructName<T, sfinae::Has_CURRENT_EXPORTED_STRUCT_NAME<T>::value>::DoIt();
  }
};

template <NameFormat NF, typename T>
struct CurrentVariantTypeNameImpl;

template <NameFormat NF, typename NAME, typename... TS>
struct CurrentVariantTypeNameImpl<NF, VariantImpl<NAME, TypeListImpl<TS...>>> {
  static std::string DoIt() { return NAME::CustomVariantName(); }
};

template <typename... TS>
struct CurrentVariantTypeNameImpl<NameFormat::Z, VariantImpl<CurrentVariantDefaultName, TypeListImpl<TS...>>> {
  static std::string DoIt() {
    return std::string("Variant_B_") + JoinTypeNames<NameFormat::Z, TypeListImpl<TS...>>::Join('_') + "_E";
  }
};

template <typename... TS>
struct CurrentVariantTypeNameImpl<NameFormat::DebugDump, VariantImpl<CurrentVariantDefaultName, TypeListImpl<TS...>>> {
  static std::string DoIt() {
    return std::string("Variant<") + JoinTypeNames<NameFormat::DebugDump, TypeListImpl<TS...>>::Join(", ") + '>';
  }
};

template <typename... TS>
struct CurrentVariantTypeNameImpl<NameFormat::AsIdentifier,
                                  VariantImpl<CurrentVariantDefaultName, TypeListImpl<TS...>>> {
  static std::string DoIt() {
    return std::string("Variant_") + JoinTypeNames<NameFormat::AsIdentifier, TypeListImpl<TS...>>::Join('_');
  }
};

template <NameFormat NF, typename T>
struct CurrentTypeNameImpl<NF, T, false, true, false> {
  static std::string GetCurrentTypeName() { return CurrentVariantTypeNameImpl<NF, T>::DoIt(); }
};

template <NameFormat NF, typename T>
struct CurrentTypeNameImpl<NF, T, false, false, true> {
  static std::string GetCurrentTypeName() { return reflection::EnumName<T>(); }
};

#define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, cpp_type, current_type, fs_type, md_type, typescript_type) \
  template <NameFormat NF>                                                                                      \
  struct CurrentTypeNameImpl<NF, cpp_type, false, false, false> {                                               \
    static const char* GetCurrentTypeName() { return #cpp_type; }                                               \
  };                                                                                                            \
  template <>                                                                                                   \
  struct CurrentTypeNameImpl<NameFormat::AsIdentifier, cpp_type, false, false, false> {                         \
    static const char* GetCurrentTypeName() { return #current_type; }                                           \
  };
#include "primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

template <NameFormat NF, typename T>
struct CurrentTypeNameImpl<NF, std::vector<T>, false, false, false> {
  static std::string GetCurrentTypeName() {
    return std::string("std::vector<") + CurrentTypeNameCaller<NF, T>::CallGetCurrentTypeName() + '>';
  }
};

template <NameFormat NF, typename T>
struct CurrentTypeNameImpl<NF, std::set<T>, false, false, false> {
  static std::string GetCurrentTypeName() {
    return std::string("std::set<") + CurrentTypeNameCaller<NF, T>::CallGetCurrentTypeName() + '>';
  }
};

template <NameFormat NF, typename T>
struct CurrentTypeNameImpl<NF, std::unordered_set<T>, false, false, false> {
  static std::string GetCurrentTypeName() {
    return std::string("std::unordered_set<") + CurrentTypeNameCaller<NF, T>::CallGetCurrentTypeName() + '>';
  }
};

template <NameFormat NF, typename TF, typename TS>
struct CurrentTypeNameImpl<NF, std::pair<TF, TS>, false, false, false> {
  static std::string GetCurrentTypeName() {
    return std::string("std::pair<") + CurrentTypeNameCaller<NF, TF>::CallGetCurrentTypeName() + ", " +
           CurrentTypeNameCaller<NF, TS>::CallGetCurrentTypeName() + '>';
  }
};

template <NameFormat NF, typename TK, typename TV>
struct CurrentTypeNameImpl<NF, std::map<TK, TV>, false, false, false> {
  static std::string GetCurrentTypeName() {
    return std::string("std::map<") + CurrentTypeNameCaller<NF, TK>::CallGetCurrentTypeName() + ", " +
           CurrentTypeNameCaller<NF, TV>::CallGetCurrentTypeName() + '>';
  }
};

template <NameFormat NF, typename TK, typename TV>
struct CurrentTypeNameImpl<NF, std::unordered_map<TK, TV>, false, false, false> {
  static std::string GetCurrentTypeName() {
    return std::string("std::unordered_map<") + CurrentTypeNameCaller<NF, TK>::CallGetCurrentTypeName() + ", " +
           CurrentTypeNameCaller<NF, TV>::CallGetCurrentTypeName() + '>';
  }
};

template <NameFormat NF, typename T>
struct CurrentTypeNameImpl<NF, Optional<T>, false, false, false> {
  static std::string GetCurrentTypeName() {
    return std::string("Optional<") + CurrentTypeNameCaller<NF, T>::CallGetCurrentTypeName() + '>';
  }
};

// Names for `AsIdentifier` name decoration.
template <typename T>
struct CurrentTypeNameImpl<NameFormat::AsIdentifier, std::vector<T>, false, false, false> {
  static std::string GetCurrentTypeName() {
    return std::string("Vector_") + CurrentTypeNameCaller<NameFormat::AsIdentifier, T>::CallGetCurrentTypeName();
  }
};

template <typename T>
struct CurrentTypeNameImpl<NameFormat::AsIdentifier, std::set<T>, false, false, false> {
  static std::string GetCurrentTypeName() {
    return std::string("Set_") + CurrentTypeNameCaller<NameFormat::AsIdentifier, T>::CallGetCurrentTypeName();
  }
};

template <typename T>
struct CurrentTypeNameImpl<NameFormat::AsIdentifier, std::unordered_set<T>, false, false, false> {
  static std::string GetCurrentTypeName() {
    return std::string("UnorderedSet_") + CurrentTypeNameCaller<NameFormat::AsIdentifier, T>::CallGetCurrentTypeName();
  }
};

template <typename TF, typename TS>
struct CurrentTypeNameImpl<NameFormat::AsIdentifier, std::pair<TF, TS>, false, false, false> {
  static std::string GetCurrentTypeName() {
    return std::string("Pair_") + CurrentTypeNameCaller<NameFormat::AsIdentifier, TF>::CallGetCurrentTypeName() + '_' +
           CurrentTypeNameCaller<NameFormat::AsIdentifier, TS>::CallGetCurrentTypeName();
  }
};

template <typename TK, typename TV>
struct CurrentTypeNameImpl<NameFormat::AsIdentifier, std::map<TK, TV>, false, false, false> {
  static std::string GetCurrentTypeName() {
    return std::string("Map_") + CurrentTypeNameCaller<NameFormat::AsIdentifier, TK>::CallGetCurrentTypeName() + '_' +
           CurrentTypeNameCaller<NameFormat::AsIdentifier, TV>::CallGetCurrentTypeName();
  }
};

template <typename TK, typename TV>
struct CurrentTypeNameImpl<NameFormat::AsIdentifier, std::unordered_map<TK, TV>, false, false, false> {
  static std::string GetCurrentTypeName() {
    return std::string("UnorderedMap_") +
           CurrentTypeNameCaller<NameFormat::AsIdentifier, TK>::CallGetCurrentTypeName() + '_' +
           CurrentTypeNameCaller<NameFormat::AsIdentifier, TV>::CallGetCurrentTypeName();
  }
};

template <typename T>
struct CurrentTypeNameImpl<NameFormat::AsIdentifier, Optional<T>, false, false, false> {
  static std::string GetCurrentTypeName() {
    return std::string("Optional_") + CurrentTypeNameCaller<NameFormat::AsIdentifier, T>::CallGetCurrentTypeName();
  }
};

template <NameFormat NF, typename T, typename T_INNER>
struct CurrentPossiblyTemplatedStructName {
  static std::string DoIt(const char* mid, const char* end) {
    std::string result = CallRightGetCurrentStructName<T, sfinae::Has_CURRENT_EXPORTED_STRUCT_NAME<T>::value>::DoIt();
    CURRENT_ASSERT(result.length() >= 3);
    CURRENT_ASSERT(result.substr(result.length() - 2) == "_Z");
    return result.substr(0, result.length() - 2) + mid + CurrentTypeNameCaller<NF, T_INNER>::CallGetCurrentTypeName() +
           end;
  }
};

template <NameFormat NF, typename T>
struct CurrentPossiblyTemplatedStructName<NF, T, void> {
  static std::string DoIt(const char* mid, const char* end) {
    static_cast<void>(mid);
    static_cast<void>(end);
    return std::string(CallRightGetCurrentStructName<T, sfinae::Has_CURRENT_EXPORTED_STRUCT_NAME<T>::value>::DoIt());
  }
};

template <typename T>
struct CurrentTypeNameImpl<NameFormat::DebugDump, T, true, false, false> {
  static std::string GetCurrentTypeName() {
    return CurrentPossiblyTemplatedStructName<NameFormat::DebugDump, T, reflection::TemplateInnerType<T>>::DoIt("<",
                                                                                                                ">");
  }
};

template <typename T>
struct CurrentTypeNameImpl<NameFormat::AsIdentifier, T, true, false, false> {
  static std::string GetCurrentTypeName() {
    return CurrentPossiblyTemplatedStructName<NameFormat::AsIdentifier, T, reflection::TemplateInnerType<T>>::DoIt(
        "_T_", "");
  }
};

}  // namespace current::reflection::impl

template <typename T, NameFormat NF = NameFormat::DebugDump>
inline const char* CurrentTypeName() {
  return impl::CurrentTypeNameCaller<NF, T>::CallGetCurrentTypeName();
}

namespace impl {

template <NameFormat NF, typename T>
struct JoinTypeNames<NF, TypeListImpl<T>> {
  template <typename SEPARATOR>
  static const char* Join(SEPARATOR&&) {
    return CurrentTypeNameCaller<NF, T>::CallGetCurrentTypeName();
  }
};

template <NameFormat NF, typename T, typename... TS>
struct JoinTypeNames<NF, TypeListImpl<T, TS...>> {
  template <typename SEPARATOR>
  static std::string Join(SEPARATOR&& separator) {
    return std::string(CurrentTypeNameCaller<NF, T>::CallGetCurrentTypeName()) + separator +
           JoinTypeNames<NF, TypeListImpl<TS...>>::Join(std::forward<SEPARATOR>(separator));
  }
};

}  // namespace current::reflection::impl

}  // namespace current::reflection
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_TYPENAME_H
