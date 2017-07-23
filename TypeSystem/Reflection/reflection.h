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

#ifndef CURRENT_TYPE_SYSTEM_REFLECTION_REFLECTION_H
#define CURRENT_TYPE_SYSTEM_REFLECTION_REFLECTION_H

#include <map>
#include <set>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>

#include "exceptions.h"
#include "types.h"

#include "../optional.h"
#include "../variant.h"
#include "../struct.h"
#include "../timestamp.h"
#include "../typename.h"

#include "../../Bricks/template/call_all_constructors.h"
#include "../../Bricks/util/comparators.h"
#include "../../Bricks/util/singleton.h"

namespace current {

namespace reflection {

namespace impl {
template <>
struct CurrentTypeNameImpl<reflection::TypeID, false, false, true> {
  static const char* GetCurrentTypeName() { return "TypeID"; }
};
}  // namespace current::reflection::impl

template <typename T_TOP_LEVEL, typename T_CURRENT>
struct RecursiveTypeTraverser;

template <typename T_TOP_LEVEL, typename T_TYPE = T_TOP_LEVEL>
reflection::TypeID CurrentTypeID() {
  return ThreadLocalSingleton<reflection::RecursiveTypeTraverser<T_TOP_LEVEL, T_TYPE>>().ComputeTypeID();
}

// `CurrentTypeID<T, T>()` is the "user-facing" type ID of `T`, whereas for each individual `T` the values
// of `CurrentTypeID<TOP_LEVEL, T>()` may and will be different in case of cyclical dependencies, as the order
// of their resolution by definition depends on which part of the cycle was the stating point.

template <typename T_TOP_LEVEL, typename T_STRUCT>
struct StructFieldsTraverser {
  using fields_list_t = std::vector<ReflectedType_Struct_Field>;

  explicit StructFieldsTraverser(fields_list_t& fields) : fields_(fields) { CURRENT_ASSERT(fields_.empty()); }

  template <typename T, int I>
  void operator()(TypeSelector<T>, const std::string& name, SimpleIndex<I>) const {
    const char* retrieved_description = FieldDescriptions::template Description<T_STRUCT, I>();
    Optional<std::string> description;
    if (retrieved_description) {
      description = retrieved_description;
    }
    // If this call to `CurrentTypeID()` returns `TypeID::UninitializedType`, the forthcoming call
    // to `CalculateTypeID` will use the field's name instead, as the cycle has to be broken.
    // This is the intended behavior, and that's how it should be. -- D.K.
    fields_.emplace_back(CurrentTypeID<T_TOP_LEVEL, T>(), name, description);
  }

 private:
  fields_list_t& fields_;
};

// To make `std::type_index` legal to use as map values.
struct DefaultConstructibleTypeIndex {
  std::type_index type_index = std::type_index(typeid(void));
  void operator=(const std::type_index new_value) { type_index = new_value; }
  bool operator==(DefaultConstructibleTypeIndex rhs) const { return type_index == rhs.type_index; }
};

// `TopLevelTypeTraverser` is constructed as the `ThreadLocalSingleton` from `RecursiveTypeTraverser<T, *>`,
// which is invoked when `CurrentTypeID<T>()` is called.
template <typename T_TOP_LEVEL>
struct TopLevelTypeTraverser {
  std::unordered_map<std::type_index, std::string> cpp2name;
  std::unordered_map<std::type_index, ReflectedType> cpp2current;
  std::unordered_map<TypeID, DefaultConstructibleTypeIndex, CurrentHashFunction<TypeID>> current2cpp;

#define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, cpp_type, current_type, fs_type, md_type, typescript_type) \
  ReflectedType operator()(TypeSelector<cpp_type>) {                                                            \
    return ReflectedType(ReflectedType_Primitive(TypeID::current_type));                                        \
  }
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

  template <typename T>
  std::enable_if_t<std::is_enum<T>::value, ReflectedType> operator()(TypeSelector<T>) {
    return ReflectedType(
        ReflectedType_Enum(EnumName<T>(), CurrentTypeID<T_TOP_LEVEL, typename std::underlying_type<T>::type>()));
  }

  template <typename T>
  ReflectedType operator()(TypeSelector<std::vector<T>>) {
    ReflectedType_Vector result(CurrentTypeID<T_TOP_LEVEL, T>());
    result.type_id = CalculateTypeID(result);
    return ReflectedType(std::move(result));
  }

  template <typename TF, typename TS>
  ReflectedType operator()(TypeSelector<std::pair<TF, TS>>) {
    ReflectedType_Pair result(CurrentTypeID<T_TOP_LEVEL, TF>(), CurrentTypeID<T_TOP_LEVEL, TS>());
    result.type_id = CalculateTypeID(result);
    return ReflectedType(std::move(result));
  }

  template <typename TK, typename TV>
  ReflectedType operator()(TypeSelector<std::map<TK, TV>>) {
    ReflectedType_Map result(CurrentTypeID<T_TOP_LEVEL, TK>(), CurrentTypeID<T_TOP_LEVEL, TV>());
    result.type_id = CalculateTypeID(result);
    return ReflectedType(std::move(result));
  }

  template <typename TK, typename TV>
  ReflectedType operator()(TypeSelector<std::unordered_map<TK, TV>>) {
    ReflectedType_UnorderedMap result(CurrentTypeID<T_TOP_LEVEL, TK>(), CurrentTypeID<T_TOP_LEVEL, TV>());
    result.type_id = CalculateTypeID(result);
    return ReflectedType(std::move(result));
  }

  template <typename T>
  ReflectedType operator()(TypeSelector<std::set<T>>) {
    ReflectedType_Set result(CurrentTypeID<T_TOP_LEVEL, T>());
    result.type_id = CalculateTypeID(result);
    return ReflectedType(std::move(result));
  }

  template <typename T>
  ReflectedType operator()(TypeSelector<std::unordered_set<T>>) {
    ReflectedType_UnorderedSet result(CurrentTypeID<T_TOP_LEVEL, T>());
    result.type_id = CalculateTypeID(result);
    return ReflectedType(std::move(result));
  }

  template <typename T>
  ReflectedType operator()(TypeSelector<Optional<T>>) {
    ReflectedType_Optional result(CurrentTypeID<T_TOP_LEVEL, T>());
    result.type_id = CalculateTypeID(result);
    return ReflectedType(std::move(result));
  }

  template <typename CASE>
  struct ReflectVariantCase {
    ReflectVariantCase(ReflectedType_Variant& destination) {
      destination.cases.push_back(CurrentTypeID<T_TOP_LEVEL, CASE>());
    }
  };

  template <typename NAME, typename... TS>
  ReflectedType operator()(TypeSelector<VariantImpl<NAME, TypeListImpl<TS...>>>) {
    ReflectedType_Variant result;
    result.name = VariantImpl<NAME, TypeListImpl<TS...>>::VariantName();
    current::metaprogramming::call_all_constructors_with<ReflectVariantCase,
                                                         ReflectedType_Variant,
                                                         TypeListImpl<TS...>>(result);
    result.type_id = CalculateTypeID(result);
    return ReflectedType(std::move(result));
  }

  template <typename T>
  std::enable_if_t<IS_CURRENT_STRUCT(T), ReflectedType> operator()(TypeSelector<T>) {
    ReflectedType_Struct s;
    s.native_name = CurrentTypeName<T>();
    s.super_id = ReflectSuper<T>();
    s.template_id = ReflectTemplateInnerType<T>();
    VisitAllFields<T, FieldTypeAndNameAndIndex>::WithoutObject(StructFieldsTraverser<T_TOP_LEVEL, T>(s.fields));
    s.type_id = CalculateTypeID(s);
    return s;
  }

 private:
  template <typename T>
  std::enable_if_t<std::is_same<SuperType<T>, CurrentStruct>::value, TypeID> ReflectSuper() {
    return TypeID::CurrentStruct;
  }

  template <typename T>
  std::enable_if_t<!std::is_same<SuperType<T>, CurrentStruct>::value, TypeID> ReflectSuper() {
    return CurrentTypeID<T_TOP_LEVEL, SuperType<T>>();
  }

  template <typename T>
  std::enable_if_t<std::is_same<TemplateInnerType<T>, void>::value, Optional<TypeID>> ReflectTemplateInnerType() {
    return nullptr;
  }

  template <typename T>
  std::enable_if_t<!std::is_same<TemplateInnerType<T>, void>::value, Optional<TypeID>> ReflectTemplateInnerType() {
    return CurrentTypeID<T_TOP_LEVEL, TemplateInnerType<T>>();
  }
};

template <typename T_TOP_LEVEL, typename T_CURRENT>
struct RecursiveTypeTraverser {
  TopLevelTypeTraverser<T_TOP_LEVEL>& top = ThreadLocalSingleton<TopLevelTypeTraverser<T_TOP_LEVEL>>();
  TypeID resulting_type_id = TypeID::UninitializedType;

  TypeID ComputeTypeID() {
    if (resulting_type_id == TypeID::UninitializedType) {
      const uint64_t hash = current::CRC32(CurrentTypeName<T_CURRENT>());
      resulting_type_id = static_cast<TypeID>(TYPEID_CYCLIC_DEPENDENCY_TYPE + hash % TYPEID_TYPE_RANGE);

      // Relying on the singleton to instantiate this type exactly once, so in case of a re-entrant call
      // the original value of `resulting_type_id`, which is `TypeID::UninitializedType`, will be returned.
      const std::type_index type_index = std::type_index(typeid(T_CURRENT));
      if (!top.cpp2current.count(type_index)) {
        top.cpp2name[type_index] = CurrentTypeName<T_CURRENT>();
        top.cpp2current.insert(std::make_pair(type_index, top(TypeSelector<T_CURRENT>())));
        top.current2cpp[Value<ReflectedTypeBase>(top.cpp2current.at(type_index)).type_id] = type_index;
      }
      resulting_type_id = Value<ReflectedTypeBase>(top.cpp2current.at(type_index)).type_id;
    }
    return resulting_type_id;
  }
};

// `ReflectorImpl` is a thread-local singleton to generate reflected types metadata at runtime.
struct ReflectorImpl {
  static ReflectorImpl& Reflector() { return ThreadLocalSingleton<ReflectorImpl>(); }

  template <typename T>
  const ReflectedType& ReflectType() {
    // Trigger the global (not thread-local!) singletons to fill in the internal structures.
    const TypeID type_id = CurrentTypeID<T, T>();

    // Augment the set of types this thread-local instance hsa the knowledge of, to preserve
    // the invariant that it always contains the transitive closure of the types added so far
    // and their dependencies.
    const auto& types = ThreadLocalSingleton<TopLevelTypeTraverser<T>>();
    for (const auto& kv : types.cpp2name) {
      if (!type_names_.count(kv.first)) {
        type_names_.insert(kv);
      } else {
        if (type_names_[kv.first] != kv.second) {
          std::cerr << "Type matching error when reflecting on " << CurrentTypeName<T>() << " : current name "
                    << kv.second << ", previous name " << type_names_[kv.first] << std::endl;
        }
        CURRENT_ASSERT(type_names_[kv.first] == kv.second);
      }
    }
    for (const auto& kv : types.cpp2current) {
      if (!reflected_cpp_types_.count(kv.first)) {
        reflected_cpp_types_.insert(kv);
      } else {
        CURRENT_ASSERT(reflected_cpp_types_[kv.first] == kv.second);
      }
    }
    for (const auto& kv : types.current2cpp) {
      if (!type_index_by_type_id_.count(kv.first)) {
        type_index_by_type_id_.insert(kv);
      } else {
        if (!(type_index_by_type_id_[kv.first] == kv.second) &&
            !(types.cpp2name.at(kv.second.type_index) == type_names_[type_index_by_type_id_[kv.first].type_index])) {
          std::cerr << "Type matching error when reflecting on " << CurrentTypeName<T>() << ' '
                    << types.cpp2name.at(kv.second.type_index)
                    << " != " << type_names_[type_index_by_type_id_[kv.first].type_index] << std::endl;
        }
        CURRENT_ASSERT(type_index_by_type_id_[kv.first] == kv.second ||
                       types.cpp2name.at(kv.second.type_index) ==
                           type_names_[type_index_by_type_id_[kv.first].type_index]);
      }
    }

    // Return what the user has requested.
    return ReflectedTypeByTypeID(type_id);
  }

  const ReflectedType& ReflectedTypeByTypeID(const TypeID type_id) const {
    const auto cit = type_index_by_type_id_.find(type_id);
    if (cit != type_index_by_type_id_.end()) {
      return reflected_cpp_types_.at(cit->second.type_index);
    } else {
      CURRENT_THROW(UnknownTypeIDException());  // LCOV_EXCL_LINE
    }
  }

  size_t KnownTypesCountForUnitTest() const { return reflected_cpp_types_.size(); }

 private:
  std::unordered_map<std::type_index, std::string> type_names_;  // Mostly for debugging purposes. -- D.K.
  std::unordered_map<std::type_index, ReflectedType> reflected_cpp_types_;
  std::unordered_map<TypeID, DefaultConstructibleTypeIndex, CurrentHashFunction<TypeID>> type_index_by_type_id_;
};

inline ReflectorImpl& Reflector() { return ReflectorImpl::Reflector(); }

}  // namespace reflection
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_REFLECTION_REFLECTION_H
