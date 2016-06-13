// The `current.h` file is the one from `https://github.com/C5T/Current`.
// Compile with `-std=c++11` or higher.

#ifndef CURRENT_USERSPACE_805F01FE9B050632
#define CURRENT_USERSPACE_805F01FE9B050632

#include "current.h"

// clang-format off

namespace current_userspace_805f01fe9b050632 {

CURRENT_STRUCT(SimpleStruct) {
  CURRENT_FIELD(x, int32_t);
  CURRENT_FIELD(y, int32_t);
  CURRENT_FIELD(z, std::string);
};
using T9200560626419622480 = SimpleStruct;

CURRENT_STRUCT(StructWithStruct) {
  CURRENT_FIELD(s, SimpleStruct);
};
using T9200847454180217914 = StructWithStruct;

CURRENT_VARIANT(Variant_B_SimpleStruct_StructWithStruct_E, SimpleStruct, StructWithStruct);
using T9221517619468071326 = Variant_B_SimpleStruct_StructWithStruct_E;

CURRENT_STRUCT(StructWithVariant) {
  CURRENT_FIELD(v, Variant_B_SimpleStruct_StructWithStruct_E);
};
using T9200946838064757591 = StructWithVariant;

CURRENT_STRUCT(Name) {
  CURRENT_FIELD(first, std::string);
  CURRENT_FIELD(last, std::string);
};
using T9203533648527088493 = Name;

CURRENT_STRUCT(StructWithVectorOfNames) {
  CURRENT_FIELD(w, std::vector<Name>);
};
using T9204974803222981782 = StructWithVectorOfNames;

CURRENT_ENUM(EnumClassType, bool) {};
using T9010000003056575809 = EnumClassType;

CURRENT_STRUCT(LeftoverTypesToMakeSureTypeEvolutionHasThemAmm) {
  CURRENT_FIELD(enum_class, EnumClassType);
  CURRENT_FIELD(optional, Optional<std::string>);
};
using T9201362264147007706 = LeftoverTypesToMakeSureTypeEvolutionHasThemAmm;

CURRENT_VARIANT(Variant_B_Name_LeftoverTypesToMakeSureTypeEvolutionHasThemAmm_E, Name, LeftoverTypesToMakeSureTypeEvolutionHasThemAmm);
using T9228209317326277871 = Variant_B_Name_LeftoverTypesToMakeSureTypeEvolutionHasThemAmm_E;

CURRENT_STRUCT(StructWithVectorOfVariants) {
  CURRENT_FIELD(z, std::vector<Variant_B_Name_LeftoverTypesToMakeSureTypeEvolutionHasThemAmm_E>);
};
using T9204943195437374945 = StructWithVectorOfVariants;

}  // namespace current_userspace_805f01fe9b050632

CURRENT_NAMESPACE(USERSPACE_805F01FE9B050632) {
  CURRENT_NAMESPACE_TYPE(EnumClassType, current_userspace_805f01fe9b050632::EnumClassType);
  CURRENT_NAMESPACE_TYPE(SimpleStruct, current_userspace_805f01fe9b050632::SimpleStruct);
  CURRENT_NAMESPACE_TYPE(StructWithStruct, current_userspace_805f01fe9b050632::StructWithStruct);
  CURRENT_NAMESPACE_TYPE(StructWithVariant, current_userspace_805f01fe9b050632::StructWithVariant);
  CURRENT_NAMESPACE_TYPE(LeftoverTypesToMakeSureTypeEvolutionHasThemAmm, current_userspace_805f01fe9b050632::LeftoverTypesToMakeSureTypeEvolutionHasThemAmm);
  CURRENT_NAMESPACE_TYPE(Name, current_userspace_805f01fe9b050632::Name);
  CURRENT_NAMESPACE_TYPE(StructWithVectorOfVariants, current_userspace_805f01fe9b050632::StructWithVectorOfVariants);
  CURRENT_NAMESPACE_TYPE(StructWithVectorOfNames, current_userspace_805f01fe9b050632::StructWithVectorOfNames);
  CURRENT_NAMESPACE_TYPE(Variant_B_SimpleStruct_StructWithStruct_E, current_userspace_805f01fe9b050632::Variant_B_SimpleStruct_StructWithStruct_E);
  CURRENT_NAMESPACE_TYPE(Variant_B_Name_LeftoverTypesToMakeSureTypeEvolutionHasThemAmm_E, current_userspace_805f01fe9b050632::Variant_B_Name_LeftoverTypesToMakeSureTypeEvolutionHasThemAmm_E);
};

namespace current {
namespace type_evolution {

// Default evolution for `CURRENT_ENUM(EnumClassType)`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_805F01FE9B050632::EnumClassType, EVOLUTOR> {
  template <typename INTO>
  static void Go(USERSPACE_805F01FE9B050632::EnumClassType from,
                 typename INTO::EnumClassType& into) {
    into = from;
  }
};

// Default evolution for struct `SimpleStruct`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_805F01FE9B050632::SimpleStruct, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_805F01FE9B050632, CHECK>::value>>
  static void Go(const typename NAMESPACE::SimpleStruct& from,
                 typename INTO::SimpleStruct& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_805F01FE9B050632::SimpleStruct>::value == 3,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.x), EVOLUTOR>::template Go<INTO>(from.x, into.x);
      Evolve<NAMESPACE, decltype(from.y), EVOLUTOR>::template Go<INTO>(from.y, into.y);
      Evolve<NAMESPACE, decltype(from.z), EVOLUTOR>::template Go<INTO>(from.z, into.z);
  }
};

// Default evolution for struct `StructWithStruct`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_805F01FE9B050632::StructWithStruct, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_805F01FE9B050632, CHECK>::value>>
  static void Go(const typename NAMESPACE::StructWithStruct& from,
                 typename INTO::StructWithStruct& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_805F01FE9B050632::StructWithStruct>::value == 1,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.s), EVOLUTOR>::template Go<INTO>(from.s, into.s);
  }
};

// Default evolution for struct `StructWithVariant`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_805F01FE9B050632::StructWithVariant, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_805F01FE9B050632, CHECK>::value>>
  static void Go(const typename NAMESPACE::StructWithVariant& from,
                 typename INTO::StructWithVariant& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_805F01FE9B050632::StructWithVariant>::value == 1,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.v), EVOLUTOR>::template Go<INTO>(from.v, into.v);
  }
};

// Default evolution for struct `LeftoverTypesToMakeSureTypeEvolutionHasThemAmm`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_805F01FE9B050632::LeftoverTypesToMakeSureTypeEvolutionHasThemAmm, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_805F01FE9B050632, CHECK>::value>>
  static void Go(const typename NAMESPACE::LeftoverTypesToMakeSureTypeEvolutionHasThemAmm& from,
                 typename INTO::LeftoverTypesToMakeSureTypeEvolutionHasThemAmm& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_805F01FE9B050632::LeftoverTypesToMakeSureTypeEvolutionHasThemAmm>::value == 2,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.enum_class), EVOLUTOR>::template Go<INTO>(from.enum_class, into.enum_class);
      Evolve<NAMESPACE, decltype(from.optional), EVOLUTOR>::template Go<INTO>(from.optional, into.optional);
  }
};

// Default evolution for struct `Name`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_805F01FE9B050632::Name, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_805F01FE9B050632, CHECK>::value>>
  static void Go(const typename NAMESPACE::Name& from,
                 typename INTO::Name& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_805F01FE9B050632::Name>::value == 2,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.first), EVOLUTOR>::template Go<INTO>(from.first, into.first);
      Evolve<NAMESPACE, decltype(from.last), EVOLUTOR>::template Go<INTO>(from.last, into.last);
  }
};

// Default evolution for struct `StructWithVectorOfVariants`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_805F01FE9B050632::StructWithVectorOfVariants, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_805F01FE9B050632, CHECK>::value>>
  static void Go(const typename NAMESPACE::StructWithVectorOfVariants& from,
                 typename INTO::StructWithVectorOfVariants& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_805F01FE9B050632::StructWithVectorOfVariants>::value == 1,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.z), EVOLUTOR>::template Go<INTO>(from.z, into.z);
  }
};

// Default evolution for struct `StructWithVectorOfNames`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_805F01FE9B050632::StructWithVectorOfNames, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_805F01FE9B050632, CHECK>::value>>
  static void Go(const typename NAMESPACE::StructWithVectorOfNames& from,
                 typename INTO::StructWithVectorOfNames& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_805F01FE9B050632::StructWithVectorOfNames>::value == 1,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.w), EVOLUTOR>::template Go<INTO>(from.w, into.w);
  }
};

// Default evolution for `Variant<SimpleStruct, StructWithStruct>`.
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_805F01FE9B050632_Variant_B_SimpleStruct_StructWithStruct_E_Cases {
  DST& into;
  explicit USERSPACE_805F01FE9B050632_Variant_B_SimpleStruct_StructWithStruct_E_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::SimpleStruct& value) const {
    using into_t = typename INTO::SimpleStruct;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::SimpleStruct, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::StructWithStruct& value) const {
    using into_t = typename INTO::StructWithStruct;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::StructWithStruct, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename NAMESPACE, typename EVOLUTOR, typename VARIANT_NAME_HELPER>
struct Evolve<NAMESPACE, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_805F01FE9B050632::SimpleStruct, USERSPACE_805F01FE9B050632::StructWithStruct>>, EVOLUTOR> {
  // TODO(dkorolev): A `static_assert` to ensure the number of cases is the same.
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_805F01FE9B050632, CHECK>::value>>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_805F01FE9B050632::SimpleStruct, USERSPACE_805F01FE9B050632::StructWithStruct>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(USERSPACE_805F01FE9B050632_Variant_B_SimpleStruct_StructWithStruct_E_Cases<decltype(into), NAMESPACE, INTO, EVOLUTOR>(into));
  }
};

// Default evolution for `Variant<Name, LeftoverTypesToMakeSureTypeEvolutionHasThemAmm>`.
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_805F01FE9B050632_Variant_B_Name_LeftoverTypesToMakeSureTypeEvolutionHasThemAmm_E_Cases {
  DST& into;
  explicit USERSPACE_805F01FE9B050632_Variant_B_Name_LeftoverTypesToMakeSureTypeEvolutionHasThemAmm_E_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::Name& value) const {
    using into_t = typename INTO::Name;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::Name, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::LeftoverTypesToMakeSureTypeEvolutionHasThemAmm& value) const {
    using into_t = typename INTO::LeftoverTypesToMakeSureTypeEvolutionHasThemAmm;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::LeftoverTypesToMakeSureTypeEvolutionHasThemAmm, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename NAMESPACE, typename EVOLUTOR, typename VARIANT_NAME_HELPER>
struct Evolve<NAMESPACE, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_805F01FE9B050632::Name, USERSPACE_805F01FE9B050632::LeftoverTypesToMakeSureTypeEvolutionHasThemAmm>>, EVOLUTOR> {
  // TODO(dkorolev): A `static_assert` to ensure the number of cases is the same.
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_805F01FE9B050632, CHECK>::value>>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_805F01FE9B050632::Name, USERSPACE_805F01FE9B050632::LeftoverTypesToMakeSureTypeEvolutionHasThemAmm>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(USERSPACE_805F01FE9B050632_Variant_B_Name_LeftoverTypesToMakeSureTypeEvolutionHasThemAmm_E_Cases<decltype(into), NAMESPACE, INTO, EVOLUTOR>(into));
  }
};

}  // namespace current::type_evolution
}  // namespace current

// clang-format on

#endif  // CURRENT_USERSPACE_805F01FE9B050632
