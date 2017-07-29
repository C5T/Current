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

template <typename T_TOP_LEVEL, typename T_CURRENT>
struct RecursiveTypeTraverser;

}  // namespace current::reflection::impl

// `CurrentTypeID<T, T>()` is the "user-facing" type ID of `T`, whereas for each individual `T` the values
// of `CurrentTypeID<TOP_LEVEL, T>()` may and will be different in case of cyclic dependencies, as the order
// of their resolution by definition depends on which part of the cycle was the starting point.
template <typename T_TOP_LEVEL, typename T_TYPE = T_TOP_LEVEL>
reflection::TypeID CurrentTypeID() {
  return ThreadLocalSingleton<reflection::impl::RecursiveTypeTraverser<T_TOP_LEVEL, T_TYPE>>().ComputeTypeID();
}

namespace impl {

// Used as a `ThreadLocalSingleton`.
// Populated by the `ThreadLocalSingleton` of `RecursiveTypeTraverser<T_TOP_LEVEL, *>`.
template <typename T_TOP_LEVEL, typename T_STRUCT>
struct TopLevelStructFieldsTraverser {
  using fields_list_t = std::vector<ReflectedType_Struct_Field>;

  explicit TopLevelStructFieldsTraverser(fields_list_t& fields) : fields_(fields) { CURRENT_ASSERT(fields_.empty()); }

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

template <typename T_TOP_LEVEL>
struct TopLevelTypeTraverser {
#define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, cpp_type, current_type, fs_type, md_type, typescript_type) \
  TypeID operator()(TypeSelector<cpp_type>) { return TypeID::current_type; }
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

  template <typename T>
  std::enable_if_t<std::is_enum<T>::value, TypeID> operator()(TypeSelector<T>) {
    return ReflectedType_Enum(EnumName<T>(), CurrentTypeID<T_TOP_LEVEL, typename std::underlying_type<T>::type>())
        .type_id;
  }

  template <typename T>
  TypeID operator()(TypeSelector<std::vector<T>>) {
    return CalculateTypeID(ReflectedType_Vector(CurrentTypeID<T_TOP_LEVEL, T>()));
  }

  template <typename TF, typename TS>
  TypeID operator()(TypeSelector<std::pair<TF, TS>>) {
    return CalculateTypeID(ReflectedType_Pair(CurrentTypeID<T_TOP_LEVEL, TF>(), CurrentTypeID<T_TOP_LEVEL, TS>()));
  }

  template <typename TK, typename TV>
  TypeID operator()(TypeSelector<std::map<TK, TV>>) {
    return CalculateTypeID(ReflectedType_Map(CurrentTypeID<T_TOP_LEVEL, TK>(), CurrentTypeID<T_TOP_LEVEL, TV>()));
  }

  template <typename TK, typename TV>
  TypeID operator()(TypeSelector<std::unordered_map<TK, TV>>) {
    return CalculateTypeID(
        ReflectedType_UnorderedMap(CurrentTypeID<T_TOP_LEVEL, TK>(), CurrentTypeID<T_TOP_LEVEL, TV>()));
  }

  template <typename T>
  TypeID operator()(TypeSelector<std::set<T>>) {
    return CalculateTypeID(ReflectedType_Set(CurrentTypeID<T_TOP_LEVEL, T>()));
  }

  template <typename T>
  TypeID operator()(TypeSelector<std::unordered_set<T>>) {
    return CalculateTypeID(ReflectedType_UnorderedSet(CurrentTypeID<T_TOP_LEVEL, T>()));
  }

  template <typename T>
  TypeID operator()(TypeSelector<Optional<T>>) {
    return CalculateTypeID(ReflectedType_Optional(CurrentTypeID<T_TOP_LEVEL, T>()));
  }

  template <typename CASE>
  struct ReflectVariantCase {
    ReflectVariantCase(ReflectedType_Variant& destination) {
      destination.cases.push_back(CurrentTypeID<T_TOP_LEVEL, CASE>());
    }
  };

  template <typename NAME, typename... TS>
  TypeID operator()(TypeSelector<VariantImpl<NAME, TypeListImpl<TS...>>>) {
    ReflectedType_Variant result;
    // TODO(dkorolev): `CurrentTypeName` ?
    result.name = VariantImpl<NAME, TypeListImpl<TS...>>::VariantName();
    current::metaprogramming::call_all_constructors_with<ReflectVariantCase,
                                                         ReflectedType_Variant,
                                                         TypeListImpl<TS...>>(result);
    return CalculateTypeID(result);
  }

  template <typename T>
  std::enable_if_t<IS_CURRENT_STRUCT(T), TypeID> operator()(TypeSelector<T>) {
    ReflectedType_Struct result;
    result.native_name = CurrentTypeName<T>();
    result.super_id = ReflectSuper<T>();
    result.template_id = ReflectTemplateInnerType<T>();
    VisitAllFields<T, FieldTypeAndNameAndIndex>::WithoutObject(
        TopLevelStructFieldsTraverser<T_TOP_LEVEL, T>(result.fields));
    return CalculateTypeID(result);
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

 public:
  // For debugging purposes only. -- D.K.
  // TODO(dkorolev): Remove this.
  std::vector<std::type_index> types_list_;
  std::map<std::type_index, size_t> types_map_;
  template <typename T>
  void MarkTypeAsBeingConsidered() {
    const std::type_index type_index = std::type_index(typeid(T));
    size_t& index_placeholder = types_map_[type_index];
    if (!index_placeholder) {
      types_list_.push_back(type_index);
      index_placeholder = types_list_.size();
    }
    // Type `T` must be the first one considered for the top-level type `T_TOP_LEVEL`.
    if (std::is_same<T, T_TOP_LEVEL>::value == (index_placeholder > 1u)) {
      CURRENT_THROW(InternalWrongOrderReflectionException(std::string("For some reason, when reflecting on `") +
                                                          CurrentTypeName<T_TOP_LEVEL>() + "`, type `" +
                                                          CurrentTypeName<T>() + "` is being considered first."));
    }
  }
};

// Used as a `ThreadLocalSingleton`. "Reports" to a `ThreadLocalSingleton` of the top-level type.
template <typename T_TOP_LEVEL, typename T_CURRENT>
struct RecursiveTypeTraverser {
  TopLevelTypeTraverser<T_TOP_LEVEL>& top = ThreadLocalSingleton<TopLevelTypeTraverser<T_TOP_LEVEL>>();
  TypeID type_id = TypeID::UninitializedType;

  TypeID ComputeTypeID() {
    top.template MarkTypeAsBeingConsidered<T_CURRENT>();

    if (type_id == TypeID::UninitializedType) {
      const uint64_t hash = current::CRC32(CurrentTypeName<T_CURRENT>());
      type_id = static_cast<TypeID>(TYPEID_CYCLIC_DEPENDENCY_TYPE + hash % TYPEID_TYPE_RANGE);

      // Relying on the singleton to instantiate this type exactly once, so in case of a re-entrant call
      // the original value of `type_id`, which is `TypeID::UninitializedType`, will be returned.
      type_id = top(TypeSelector<T_CURRENT>());
    }

    return type_id;
  }
};

// `ReflectorImpl` is a thread-local singleton to generate reflected types metadata at runtime.
template <typename T_STRUCT>
struct ReflectorInnerStructFieldsTraverser;

struct ReflectorImpl {
  template <typename T>
  const ReflectedType& ReflectType() {
    // Fill in the internal thread-local structures for type `T` if they have not been filled yet.
    const TypeID type_id = CurrentTypeID<T>();
    Optional<ReflectedType>& optional_placeholder = map_[type_id];
    if (!Exists(optional_placeholder)) {
      optional_placeholder = std::make_unique<ReflectedType>();
      Value(optional_placeholder) = operator()(TypeSelector<T>());
    }
    return Value(optional_placeholder);
  }

  const ReflectedType& ReflectedTypeByTypeID(const TypeID type_id) const {
    const auto cit = map_.find(type_id);
    if (cit != map_.end()) {
      return Value(cit->second);
    } else {
      CURRENT_THROW(UnknownTypeIDException(current::ToString(static_cast<uint64_t>(type_id))));
    }
  }

  size_t KnownTypesCountForUnitTest() const { return map_.size(); }

#define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, cpp_type, current_type, fs_type, md_type, typescript_type) \
  ReflectedType operator()(TypeSelector<cpp_type>) {                                                            \
    return ReflectedType(ReflectedType_Primitive(TypeID::current_type));                                        \
  }
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

  template <typename T>
  std::enable_if_t<std::is_enum<T>::value, ReflectedType> operator()(TypeSelector<T>) {
    ReflectType<typename std::underlying_type<T>::type>();
    return ReflectedType(ReflectedType_Enum(EnumName<T>(), CurrentTypeID<typename std::underlying_type<T>::type>()));
  }

  template <typename T>
  ReflectedType operator()(TypeSelector<std::vector<T>>) {
    ReflectType<T>();
    ReflectedType_Vector result(CurrentTypeID<T>());
    result.type_id = CurrentTypeID<std::vector<T>>();
    return ReflectedType(std::move(result));
  }

  template <typename TF, typename TS>
  ReflectedType operator()(TypeSelector<std::pair<TF, TS>>) {
    ReflectType<TF>();
    ReflectType<TS>();
    ReflectedType_Pair result(CurrentTypeID<TF>(), CurrentTypeID<TS>());
    result.type_id = CurrentTypeID<std::pair<TF, TS>>();
    return ReflectedType(std::move(result));
  }

  template <typename TK, typename TV>
  ReflectedType operator()(TypeSelector<std::map<TK, TV>>) {
    ReflectType<TK>();
    ReflectType<TV>();
    ReflectedType_Map result(CurrentTypeID<TK>(), CurrentTypeID<TV>());
    result.type_id = CurrentTypeID<std::map<TK, TV>>();
    return ReflectedType(std::move(result));
  }

  template <typename TK, typename TV>
  ReflectedType operator()(TypeSelector<std::unordered_map<TK, TV>>) {
    ReflectType<TK>();
    ReflectType<TV>();
    ReflectedType_UnorderedMap result(CurrentTypeID<TK>(), CurrentTypeID<TV>());
    result.type_id = CurrentTypeID<std::unordered_map<TK, TV>>();
    return ReflectedType(std::move(result));
  }

  template <typename T>
  ReflectedType operator()(TypeSelector<std::set<T>>) {
    ReflectType<T>();
    ReflectedType_Set result(CurrentTypeID<T>());
    result.type_id = CurrentTypeID<std::set<T>>();
    return ReflectedType(std::move(result));
  }

  template <typename T>
  ReflectedType operator()(TypeSelector<std::unordered_set<T>>) {
    ReflectType<T>();
    ReflectedType_UnorderedSet result(CurrentTypeID<T>());
    result.type_id = CurrentTypeID<std::unordered_set<T>>();
    return ReflectedType(std::move(result));
  }

  template <typename T>
  ReflectedType operator()(TypeSelector<Optional<T>>) {
    ReflectType<T>();
    ReflectedType_Optional result(CurrentTypeID<T>());
    result.type_id = CurrentTypeID<Optional<T>>();
    return ReflectedType(std::move(result));
  }

  template <typename CASE>
  struct ReflectVariantCase {
    ReflectVariantCase(ReflectedType_Variant& destination) {
      ThreadLocalSingleton<ReflectorImpl>().ReflectType<CASE>();
      destination.cases.push_back(CurrentTypeID<CASE>());
    }
  };

  template <typename NAME, typename... TS>
  ReflectedType operator()(TypeSelector<VariantImpl<NAME, TypeListImpl<TS...>>>) {
    using T_VARIANT = VariantImpl<NAME, TypeListImpl<TS...>>;
    ReflectedType_Variant result;
    result.name = T_VARIANT::VariantName();  // TODO(dkorolev): `CurrentTypeName`?
    current::metaprogramming::call_all_constructors_with<ReflectVariantCase,
                                                         ReflectedType_Variant,
                                                         TypeListImpl<TS...>>(result);
    result.type_id = CurrentTypeID<T_VARIANT>();
    return ReflectedType(std::move(result));
  }

  template <typename T>
  std::enable_if_t<IS_CURRENT_STRUCT(T), ReflectedType> operator()(TypeSelector<T>) {
    ReflectedType_Struct s;
    s.native_name = CurrentTypeName<T>();
    s.super_id = ReflectSuper<T>();
    s.template_id = ReflectTemplateInnerType<T>();
    VisitAllFields<T, FieldTypeAndNameAndIndex>::WithoutObject(ReflectorInnerStructFieldsTraverser<T>(s.fields));
    s.type_id = CurrentTypeID<T>();
    return s;
  }

 private:
  template <typename T>
  std::enable_if_t<std::is_same<SuperType<T>, CurrentStruct>::value, TypeID> ReflectSuper() {
    return TypeID::CurrentStruct;
  }

  template <typename T>
  std::enable_if_t<!std::is_same<SuperType<T>, CurrentStruct>::value, TypeID> ReflectSuper() {
    ReflectType<SuperType<T>>();
    return CurrentTypeID<SuperType<T>>();
  }

  template <typename T>
  std::enable_if_t<std::is_same<TemplateInnerType<T>, void>::value, Optional<TypeID>> ReflectTemplateInnerType() {
    return nullptr;
  }

  template <typename T>
  std::enable_if_t<!std::is_same<TemplateInnerType<T>, void>::value, Optional<TypeID>> ReflectTemplateInnerType() {
    ReflectType<TemplateInnerType<T>>();  // NOTE(dkorolev): Unnecessary, but why not keep it in the schema output?
    return CurrentTypeID<TemplateInnerType<T>>();
  }

  // The right hand side of this `unordered_map` is to make sure the underlying instance
  // has a fixed in-memory location, allowing returning it by const reference.
  std::unordered_map<TypeID, Optional<ReflectedType>, CurrentHashFunction<TypeID>> map_;
};

template <typename T_STRUCT>
struct ReflectorInnerStructFieldsTraverser {
  using fields_list_t = std::vector<ReflectedType_Struct_Field>;

  explicit ReflectorInnerStructFieldsTraverser(fields_list_t& fields) : fields_(fields) {
    CURRENT_ASSERT(fields_.empty());
  }

  template <typename T, int I>
  void operator()(TypeSelector<T>, const std::string& name, SimpleIndex<I>) const {
    ThreadLocalSingleton<ReflectorImpl>().ReflectType<T>();

    const char* retrieved_description = FieldDescriptions::template Description<T_STRUCT, I>();
    Optional<std::string> description;
    if (retrieved_description) {
      description = retrieved_description;
    }
    // If this call to `CurrentTypeID()` returns `TypeID::UninitializedType`, the forthcoming call
    // to `CalculateTypeID` will use the field's name instead, as the cycle has to be broken.
    // This is the intended behavior, and that's how it should be. -- D.K.
    fields_.emplace_back(CurrentTypeID<T>(), name, description);
  }

 private:
  fields_list_t& fields_;
};

}  // namespace current::reflection::impl

inline impl::ReflectorImpl& Reflector() { return ThreadLocalSingleton<impl::ReflectorImpl>(); }

}  // namespace current::reflection
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_REFLECTION_REFLECTION_H
