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

#include "typeid.h"

#include <array>
#include <map>
#include <set>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include "exceptions.h"
#include "types.h"

#include "../optional.h"
#include "../struct.h"
#include "../timestamp.h"
#include "../typename.h"
#include "../variant.h"

#include "../../bricks/template/call_all_constructors.h"
#include "../../bricks/util/comparators.h"
#include "../../bricks/util/singleton.h"

namespace current {

namespace reflection {

namespace impl {
template <NameFormat NF>
struct CurrentTypeNameImpl<NF, reflection::TypeID, false, false, true, false> {
  static const char* GetCurrentTypeName() { return "TypeID"; }
};
}  // namespace impl

template <typename T_TYPE>
reflection::TypeID InternalCurrentTypeID(std::type_index top_level_type, const char* top_level_type_name);

// `CurrentTypeID<T>()` is the "user-facing" type ID of `T`, whereas for each individual `T` the values
// of correponding calls to `InternalCurrentTypeID` may and will be different in case of cyclic dependencies,
// as the order of their resolution by definition depends on which part of the cycle was the starting point.

// Called from `CurrentTypeID<T>()` defined in `typeid.h`, to be lightweight-injectable.
template <typename T>
struct DefaultCurrentTypeIDImpl final {
  static TypeID GetTypeID() {
    return InternalCurrentTypeID<T>(typeid(T), CurrentTypeName<T, NameFormat::Z>());
  }
};

#ifdef TODO_DKOROLEV_EXTRA_PARANOID_DEBUG_SYMBOL_NAME
// Unused in user code, just to cover the external safety condition LOC from the unit test. -- D.K.
template <typename T_TOP_LEVEL, typename T_TYPE>
reflection::TypeID CallCurrentTypeIDWronglyForUnitTest() {
  return InternalCurrentTypeID<T_TYPE>(typeid(T_TOP_LEVEL), CurrentTypeName<T_TOP_LEVEL, NameFormat::Z>());
}
#endif

// Stage one of two: `RecursiveTypeTraverser` and `TypeTraverserState` make sure
// the internal thread-local TypeIDs are populated correctly.
// Populated by `InternalCurrentTypeID`, used as a `ThreadLocalSingleton`.
struct RecursiveTypeTraverser {
  const std::type_index top_level_type_;
  const char* top_level_type_name_;

  template <typename T>
  TypeID CurrentTypeID_() const {
    return InternalCurrentTypeID<T>(top_level_type_, top_level_type_name_);
  }

  RecursiveTypeTraverser(std::type_index top_level_type, const char* top_level_type_name)
      : top_level_type_(top_level_type), top_level_type_name_(top_level_type_name) {}

  template <typename T_STRUCT>
  struct StructFieldsTraverser {
    const std::type_index top_level_type_;
    const char* top_level_type_name_;

    using fields_list_t = std::vector<ReflectedType_Struct_Field>;

    StructFieldsTraverser(std::type_index top_level_type, const char* top_level_type_name, fields_list_t& fields)
        : top_level_type_(top_level_type), top_level_type_name_(top_level_type_name), fields_(fields) {
      CURRENT_ASSERT(fields_.empty());
    }

    template <typename T>
    TypeID CurrentTypeID_() const {
      return InternalCurrentTypeID<T>(top_level_type_, top_level_type_name_);
    }

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
      fields_.emplace_back(CurrentTypeID_<T>(), name, description);
    }

   private:
    fields_list_t& fields_;
  };

#define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, cpp_type, current_type, fs_type, md_type, typescript_type) \
  TypeID operator()(TypeSelector<cpp_type>) { return TypeID::current_type; }
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

  template <typename T>
  std::enable_if_t<std::is_enum_v<T>, TypeID> operator()(TypeSelector<T>) {
    return ReflectedType_Enum(EnumName<T>(), CurrentTypeID_<typename std::underlying_type<T>::type>()).type_id;
  }

  template <typename T, size_t N>
  TypeID operator()(TypeSelector<std::array<T, N>>) {
    return CalculateTypeID(ReflectedType_Array(CurrentTypeID_<T>(), static_cast<uint64_t>(N)));
  }

  template <typename T>
  TypeID operator()(TypeSelector<std::vector<T>>) {
    return CalculateTypeID(ReflectedType_Vector(CurrentTypeID_<T>()));
  }

  template <typename TF, typename TS>
  TypeID operator()(TypeSelector<std::pair<TF, TS>>) {
    return CalculateTypeID(ReflectedType_Pair(CurrentTypeID_<TF>(), CurrentTypeID_<TS>()));
  }

  template <typename TK, typename TV>
  TypeID operator()(TypeSelector<std::map<TK, TV>>) {
    return CalculateTypeID(ReflectedType_Map(CurrentTypeID_<TK>(), CurrentTypeID_<TV>()));
  }

  template <typename TK, typename TV>
  TypeID operator()(TypeSelector<std::unordered_map<TK, TV>>) {
    return CalculateTypeID(ReflectedType_UnorderedMap(CurrentTypeID_<TK>(), CurrentTypeID_<TV>()));
  }

  template <typename T>
  TypeID operator()(TypeSelector<std::set<T>>) {
    return CalculateTypeID(ReflectedType_Set(CurrentTypeID_<T>()));
  }

  template <typename T>
  TypeID operator()(TypeSelector<std::unordered_set<T>>) {
    return CalculateTypeID(ReflectedType_UnorderedSet(CurrentTypeID_<T>()));
  }

  template <typename T>
  TypeID operator()(TypeSelector<Optional<T>>) {
    return CalculateTypeID(ReflectedType_Optional(CurrentTypeID_<T>()));
  }

  using VariantCaseReflectingInnerType = std::pair<ReflectedType_Variant&, std::pair<std::type_index, const char*>>;

  template <typename CASE>
  struct ReflectVariantCase {
    ReflectVariantCase(VariantCaseReflectingInnerType& data) {
      data.first.cases.push_back(InternalCurrentTypeID<CASE>(data.second.first, data.second.second));
    }
  };

  template <typename NAME, typename... TS>
  TypeID operator()(TypeSelector<VariantImpl<NAME, TypeListImpl<TS...>>>) {
    ReflectedType_Variant result;
    result.name = CurrentTypeName<VariantImpl<NAME, TypeListImpl<TS...>>, NameFormat::Z>();

    VariantCaseReflectingInnerType data(result, std::make_pair(top_level_type_, top_level_type_name_));

    current::metaprogramming::
        call_all_constructors_with<ReflectVariantCase, VariantCaseReflectingInnerType, TypeListImpl<TS...>>(data);
    return CalculateTypeID(result);
  }

  template <typename T>
  std::enable_if_t<IS_CURRENT_STRUCT(T), TypeID> operator()(TypeSelector<T>) {
    ReflectedType_Struct result;
    result.native_name = CurrentTypeName<T, NameFormat::Z>();

    result.super_id = ReflectSuper<T>();
    if (Exists(result.super_id)) {
      result.super_name = CurrentTypeName<typename T::super_t, NameFormat::Z>();
    }

    result.template_inner_id = ReflectTemplateInnerType<T>();
    if (Exists(result.template_inner_id)) {
      result.template_inner_name = CurrentTypeName<TemplateInnerType<T>, NameFormat::Z>();
    }

    VisitAllFields<T, FieldTypeAndNameAndIndex>::WithoutObject(
        StructFieldsTraverser<T>(top_level_type_, top_level_type_name_, result.fields));

    return CalculateTypeID(result);
  }

 private:
  template <typename T>
  std::enable_if_t<std::is_same_v<SuperType<T>, CurrentStruct>, Optional<TypeID>> ReflectSuper() {
    return nullptr;
  }

  template <typename T>
  std::enable_if_t<!std::is_same_v<SuperType<T>, CurrentStruct>, Optional<TypeID>> ReflectSuper() {
    return CurrentTypeID_<SuperType<T>>();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<TemplateInnerType<T>, void>, Optional<TypeID>> ReflectTemplateInnerType() {
    return nullptr;
  }

  template <typename T>
  std::enable_if_t<!std::is_same_v<TemplateInnerType<T>, void>, Optional<TypeID>> ReflectTemplateInnerType() {
    return CurrentTypeID_<TemplateInnerType<T>>();
  }
};

struct TypeTraverserState {
  RecursiveTypeTraverser traverser_;
  std::map<std::type_index, std::unique_ptr<TypeID>> cache_;

  TypeTraverserState(std::type_index top_level_type, const char* top_level_type_name)
      : traverser_(top_level_type, top_level_type_name) {}

  template <typename T>
  TypeID& GetTypeIdRefDefaultingToUninitializedType() {
    std::unique_ptr<TypeID>& placeholder = cache_[typeid(T)];
    if (!placeholder) {
      placeholder = std::make_unique<TypeID>(TypeID::UninitializedType);
    }
    return *placeholder;
  }

#ifdef TODO_DKOROLEV_EXTRA_PARANOID_DEBUG_SYMBOL_NAME
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
    if ((traverser_.top_level_type_ == typeid(T)) == (index_placeholder > 1u)) {
      CURRENT_THROW(InternalWrongOrderReflectionException(std::string("For some reason, when reflecting on `") +
                                                          traverser_.top_level_type_name_ + "`, type `" +
                                                          CurrentTypeName<T>() + "` is being considered first."));
    }
  }
#endif
};

struct TypeTraversersThreadLocalState {
 private:
  std::map<std::type_index, std::unique_ptr<TypeTraverserState>> map_;

 public:
  static TypeTraverserState& PerTypeInstance(std::type_index top_level_type, const char* top_level_type_name) {
    std::unique_ptr<TypeTraverserState>& placeholder =
        ThreadLocalSingleton<TypeTraversersThreadLocalState>().map_[top_level_type];
    if (!placeholder) {
      placeholder = std::make_unique<TypeTraverserState>(top_level_type, top_level_type_name);
    }
    return *placeholder;
  }
};

template <typename T_TYPE>
reflection::TypeID InternalCurrentTypeID(std::type_index top_level_type, const char* top_level_type_name) {
  TypeTraverserState& state = TypeTraversersThreadLocalState::PerTypeInstance(top_level_type, top_level_type_name);

  auto& type_id = state.template GetTypeIdRefDefaultingToUninitializedType<T_TYPE>();

#ifdef TODO_DKOROLEV_EXTRA_PARANOID_DEBUG_SYMBOL_NAME
  state.template MarkTypeAsBeingConsidered<T_TYPE>();
#endif

  if (type_id == TypeID::UninitializedType) {
    const uint64_t hash = current::CRC32(CurrentTypeName<T_TYPE, NameFormat::Z>());
    type_id = static_cast<TypeID>(TYPEID_CYCLIC_DEPENDENCY_TYPE + hash % TYPEID_TYPE_RANGE);

    // Relying on the singleton to instantiate this type exactly once, so in case of a re-entrant call
    // the original value of `type_id`, which is `TypeID::UninitializedType`, will be returned.
    type_id = state.traverser_(TypeSelector<T_TYPE>());
  }

  return type_id;
}

// Stage two of two: `ReflectorImpl`, or just `Reflector()` reflects on types and returns
// their info as the `ReflectedType` variant type.
// `ReflectorImpl` is a thread-local singleton to generate reflected types metadata at runtime.
struct ReflectorImpl {
  static ReflectorImpl& ThreadLocalInstance() { return ThreadLocalSingleton<ReflectorImpl>(); }

  template <typename T_STRUCT>
  struct InnerStructFieldsTraverser {
    using fields_list_t = std::vector<ReflectedType_Struct_Field>;

    explicit InnerStructFieldsTraverser(fields_list_t& fields) : fields_(fields) { CURRENT_ASSERT(fields_.empty()); }

    template <typename T, int I>
    void operator()(TypeSelector<T>, const std::string& name, SimpleIndex<I>) const {
      ThreadLocalInstance().ReflectType<T>();

      const char* retrieved_description = FieldDescriptions::template Description<T_STRUCT, I>();
      Optional<std::string> description;
      if (retrieved_description) {
        description = retrieved_description;
      }
      fields_.emplace_back(CurrentTypeID<T>(), name, description);
    }

   private:
    fields_list_t& fields_;
  };

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
  std::enable_if_t<std::is_enum_v<T>, ReflectedType> operator()(TypeSelector<T>) {
    ReflectType<typename std::underlying_type<T>::type>();
    return ReflectedType(ReflectedType_Enum(EnumName<T>(), CurrentTypeID<typename std::underlying_type<T>::type>()));
  }

  template <typename T, size_t N>
  ReflectedType operator()(TypeSelector<std::array<T, N>>) {
    ReflectType<T>();
    ReflectedType_Array result(CurrentTypeID<T>());
    result.type_id = CurrentTypeID<std::vector<T>>();
    result.size = static_cast<uint64_t>(N);
    return ReflectedType(std::move(result));
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
      ThreadLocalInstance().ReflectType<CASE>();
      destination.cases.push_back(CurrentTypeID<CASE>());
    }
  };

  template <typename NAME, typename... TS>
  ReflectedType operator()(TypeSelector<VariantImpl<NAME, TypeListImpl<TS...>>>) {
    using T_VARIANT = VariantImpl<NAME, TypeListImpl<TS...>>;
    ReflectedType_Variant result;
    result.name = CurrentTypeName<T_VARIANT, NameFormat::Z>();
    current::metaprogramming::
        call_all_constructors_with<ReflectVariantCase, ReflectedType_Variant, TypeListImpl<TS...>>(result);
    result.type_id = CurrentTypeID<T_VARIANT>();
    return ReflectedType(std::move(result));
  }

  template <typename T>
  std::enable_if_t<IS_CURRENT_STRUCT(T), ReflectedType> operator()(TypeSelector<T>) {
    ReflectedType_Struct s;
    s.native_name = CurrentTypeName<T, NameFormat::Z>();

    s.super_id = ReflectSuper<T>();
    if (Exists(s.super_id)) {
      s.super_name = CurrentTypeName<typename T::super_t, NameFormat::Z>();
    }

    s.template_inner_id = ReflectTemplateInnerType<T>();
    if (Exists(s.template_inner_id)) {
      s.template_inner_name = CurrentTypeName<TemplateInnerType<T>, NameFormat::Z>();
    }

    VisitAllFields<T, FieldTypeAndNameAndIndex>::WithoutObject(InnerStructFieldsTraverser<T>(s.fields));
    s.type_id = CurrentTypeID<T>();

    return s;
  }

 private:
  template <typename T>
  std::enable_if_t<std::is_same_v<SuperType<T>, CurrentStruct>, Optional<TypeID>> ReflectSuper() {
    return nullptr;
  }

  template <typename T>
  std::enable_if_t<!std::is_same_v<SuperType<T>, CurrentStruct>, Optional<TypeID>> ReflectSuper() {
    ReflectType<SuperType<T>>();
    return CurrentTypeID<SuperType<T>>();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<TemplateInnerType<T>, void>, Optional<TypeID>> ReflectTemplateInnerType() {
    return nullptr;
  }

  template <typename T>
  std::enable_if_t<!std::is_same_v<TemplateInnerType<T>, void>, Optional<TypeID>> ReflectTemplateInnerType() {
    ReflectType<TemplateInnerType<T>>();  // NOTE(dkorolev): Unnecessary, but why not keep it in the schema output?
    return CurrentTypeID<TemplateInnerType<T>>();
  }

  // The right hand side of this `unordered_map` is to make sure the underlying instance
  // has a fixed in-memory location, allowing returning it by const reference.
  std::unordered_map<TypeID, Optional<ReflectedType>, GenericHashFunction<TypeID>> map_;
};

inline ReflectorImpl& Reflector() { return ReflectorImpl::ThreadLocalInstance(); }

}  // namespace reflection
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_REFLECTION_REFLECTION_H
