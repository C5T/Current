// The `current.h` file is the one from `https://github.com/C5T/Current`.
// Compile with `-std=c++11` or higher.

#ifndef CURRENT_USERSPACE_F055D51FBF78DB84
#define CURRENT_USERSPACE_F055D51FBF78DB84

#include "current.h"

// clang-format off

namespace current_userspace_f055d51fbf78db84 {

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

CURRENT_ENUM(EnumClassType, bool) {};
using T9010000003056575809 = EnumClassType;

CURRENT_STRUCT(OtherTypes) {
  CURRENT_FIELD(enum_class, EnumClassType);
  CURRENT_FIELD(optional, Optional<std::string>);
};
using T9201362263922935425 = OtherTypes;

CURRENT_VARIANT(Variant_B_SimpleStruct_StructWithStruct_OtherTypes_E, SimpleStruct, StructWithStruct, OtherTypes);
using T9222804237523418434 = Variant_B_SimpleStruct_StructWithStruct_OtherTypes_E;

CURRENT_STRUCT(StructWithVariant) {
  CURRENT_FIELD(v, Variant_B_SimpleStruct_StructWithStruct_OtherTypes_E);
};
using T9202784996115577694 = StructWithVariant;

CURRENT_STRUCT(Name) {
  CURRENT_FIELD(first, std::string);
  CURRENT_FIELD(last, std::string);
};
using T9203533648527088493 = Name;

CURRENT_STRUCT(StructWithVectorOfNames) {
  CURRENT_FIELD(w, std::vector<Name>);
};
using T9204974803222981782 = StructWithVectorOfNames;

}  // namespace current_userspace_f055d51fbf78db84

CURRENT_NAMESPACE(USERSPACE_F055D51FBF78DB84) {
  CURRENT_NAMESPACE_TYPE(EnumClassType, current_userspace_f055d51fbf78db84::EnumClassType);
  CURRENT_NAMESPACE_TYPE(SimpleStruct, current_userspace_f055d51fbf78db84::SimpleStruct);
  CURRENT_NAMESPACE_TYPE(StructWithStruct, current_userspace_f055d51fbf78db84::StructWithStruct);
  CURRENT_NAMESPACE_TYPE(OtherTypes, current_userspace_f055d51fbf78db84::OtherTypes);
  CURRENT_NAMESPACE_TYPE(StructWithVariant, current_userspace_f055d51fbf78db84::StructWithVariant);
  CURRENT_NAMESPACE_TYPE(Name, current_userspace_f055d51fbf78db84::Name);
  CURRENT_NAMESPACE_TYPE(StructWithVectorOfNames, current_userspace_f055d51fbf78db84::StructWithVectorOfNames);
  CURRENT_NAMESPACE_TYPE(Variant_B_SimpleStruct_StructWithStruct_OtherTypes_E, current_userspace_f055d51fbf78db84::Variant_B_SimpleStruct_StructWithStruct_OtherTypes_E);
};  // CURRENT_NAMESPACE(USERSPACE_F055D51FBF78DB84)

namespace current {
namespace type_evolution {

// Default evolution for `CURRENT_ENUM(EnumClassType)`.
#ifndef DEFAULT_EVOLUTION_0EFC610EAA90005E980B02111A929865784B04C47419D8D3279378CAE96F393A  // USERSPACE_F055D51FBF78DB84::EnumClassType
#define DEFAULT_EVOLUTION_0EFC610EAA90005E980B02111A929865784B04C47419D8D3279378CAE96F393A  // USERSPACE_F055D51FBF78DB84::EnumClassType
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, USERSPACE_F055D51FBF78DB84::EnumClassType, EVOLUTOR> {
  template <typename INTO>
  static void Go(USERSPACE_F055D51FBF78DB84::EnumClassType from,
                 typename INTO::EnumClassType& into) {
    into = static_cast<typename INTO::EnumClassType>(from);
  }
};
#endif

// Default evolution for struct `SimpleStruct`.
#ifndef DEFAULT_EVOLUTION_9850B4E5F628696CA9E60DF29450B2961F0A045117CFDF273A060075719AB636  // typename USERSPACE_F055D51FBF78DB84::SimpleStruct
#define DEFAULT_EVOLUTION_9850B4E5F628696CA9E60DF29450B2961F0A045117CFDF273A060075719AB636  // typename USERSPACE_F055D51FBF78DB84::SimpleStruct
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_F055D51FBF78DB84::SimpleStruct, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_F055D51FBF78DB84, CHECK>::value>>
  static void Go(const typename USERSPACE_F055D51FBF78DB84::SimpleStruct& from,
                 typename INTO::SimpleStruct& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::SimpleStruct>::value == 3,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.x), EVOLUTOR>::template Go<INTO>(from.x, into.x);
      Evolve<FROM, decltype(from.y), EVOLUTOR>::template Go<INTO>(from.y, into.y);
      Evolve<FROM, decltype(from.z), EVOLUTOR>::template Go<INTO>(from.z, into.z);
  }
};
#endif

// Default evolution for struct `StructWithStruct`.
#ifndef DEFAULT_EVOLUTION_6E4B78BC03384BBFC04C0C083B1D4818552414583CEFA7969923A7AAF02AAD59  // typename USERSPACE_F055D51FBF78DB84::StructWithStruct
#define DEFAULT_EVOLUTION_6E4B78BC03384BBFC04C0C083B1D4818552414583CEFA7969923A7AAF02AAD59  // typename USERSPACE_F055D51FBF78DB84::StructWithStruct
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_F055D51FBF78DB84::StructWithStruct, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_F055D51FBF78DB84, CHECK>::value>>
  static void Go(const typename USERSPACE_F055D51FBF78DB84::StructWithStruct& from,
                 typename INTO::StructWithStruct& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::StructWithStruct>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.s), EVOLUTOR>::template Go<INTO>(from.s, into.s);
  }
};
#endif

// Default evolution for struct `OtherTypes`.
#ifndef DEFAULT_EVOLUTION_0CD0FB4488BE4FCACFEF34083301F19BD5C8A23AC7DBD12045CB31BD9E925EA2  // typename USERSPACE_F055D51FBF78DB84::OtherTypes
#define DEFAULT_EVOLUTION_0CD0FB4488BE4FCACFEF34083301F19BD5C8A23AC7DBD12045CB31BD9E925EA2  // typename USERSPACE_F055D51FBF78DB84::OtherTypes
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_F055D51FBF78DB84::OtherTypes, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_F055D51FBF78DB84, CHECK>::value>>
  static void Go(const typename USERSPACE_F055D51FBF78DB84::OtherTypes& from,
                 typename INTO::OtherTypes& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::OtherTypes>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.enum_class), EVOLUTOR>::template Go<INTO>(from.enum_class, into.enum_class);
      Evolve<FROM, decltype(from.optional), EVOLUTOR>::template Go<INTO>(from.optional, into.optional);
  }
};
#endif

// Default evolution for struct `StructWithVariant`.
#ifndef DEFAULT_EVOLUTION_047F4B4BAEB8AA23AE9D63DA1BBDE3054F370E5202C02CAC751BDE1B4087A612  // typename USERSPACE_F055D51FBF78DB84::StructWithVariant
#define DEFAULT_EVOLUTION_047F4B4BAEB8AA23AE9D63DA1BBDE3054F370E5202C02CAC751BDE1B4087A612  // typename USERSPACE_F055D51FBF78DB84::StructWithVariant
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_F055D51FBF78DB84::StructWithVariant, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_F055D51FBF78DB84, CHECK>::value>>
  static void Go(const typename USERSPACE_F055D51FBF78DB84::StructWithVariant& from,
                 typename INTO::StructWithVariant& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::StructWithVariant>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.v), EVOLUTOR>::template Go<INTO>(from.v, into.v);
  }
};
#endif

// Default evolution for struct `Name`.
#ifndef DEFAULT_EVOLUTION_AF9DF4C4F593A1A0A1E48E88D4D394E6CFEA4A1607BD341B7931182E44964756  // typename USERSPACE_F055D51FBF78DB84::Name
#define DEFAULT_EVOLUTION_AF9DF4C4F593A1A0A1E48E88D4D394E6CFEA4A1607BD341B7931182E44964756  // typename USERSPACE_F055D51FBF78DB84::Name
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_F055D51FBF78DB84::Name, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_F055D51FBF78DB84, CHECK>::value>>
  static void Go(const typename USERSPACE_F055D51FBF78DB84::Name& from,
                 typename INTO::Name& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Name>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.first), EVOLUTOR>::template Go<INTO>(from.first, into.first);
      Evolve<FROM, decltype(from.last), EVOLUTOR>::template Go<INTO>(from.last, into.last);
  }
};
#endif

// Default evolution for struct `StructWithVectorOfNames`.
#ifndef DEFAULT_EVOLUTION_3E57D01D4533C59AF50DCB73DDF7895BEFC1132BF11B3A29D4A86D97273BC22D  // typename USERSPACE_F055D51FBF78DB84::StructWithVectorOfNames
#define DEFAULT_EVOLUTION_3E57D01D4533C59AF50DCB73DDF7895BEFC1132BF11B3A29D4A86D97273BC22D  // typename USERSPACE_F055D51FBF78DB84::StructWithVectorOfNames
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_F055D51FBF78DB84::StructWithVectorOfNames, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_F055D51FBF78DB84, CHECK>::value>>
  static void Go(const typename USERSPACE_F055D51FBF78DB84::StructWithVectorOfNames& from,
                 typename INTO::StructWithVectorOfNames& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::StructWithVectorOfNames>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.w), EVOLUTOR>::template Go<INTO>(from.w, into.w);
  }
};
#endif

// Default evolution for `Variant<SimpleStruct, StructWithStruct, OtherTypes>`.
#ifndef DEFAULT_EVOLUTION_D21B58F3FF6C13FBCFB803488EAED9464A7525BBB2B7C56605C9AF3FD859413C  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_F055D51FBF78DB84::SimpleStruct, USERSPACE_F055D51FBF78DB84::StructWithStruct, USERSPACE_F055D51FBF78DB84::OtherTypes>>
#define DEFAULT_EVOLUTION_D21B58F3FF6C13FBCFB803488EAED9464A7525BBB2B7C56605C9AF3FD859413C  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_F055D51FBF78DB84::SimpleStruct, USERSPACE_F055D51FBF78DB84::StructWithStruct, USERSPACE_F055D51FBF78DB84::OtherTypes>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_F055D51FBF78DB84_Variant_B_SimpleStruct_StructWithStruct_OtherTypes_E_Cases {
  DST& into;
  explicit USERSPACE_F055D51FBF78DB84_Variant_B_SimpleStruct_StructWithStruct_OtherTypes_E_Cases(DST& into) : into(into) {}
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
  void operator()(const typename FROM_NAMESPACE::OtherTypes& value) const {
    using into_t = typename INTO::OtherTypes;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::OtherTypes, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename FROM, typename EVOLUTOR, typename VARIANT_NAME_HELPER>
struct Evolve<FROM, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_F055D51FBF78DB84::SimpleStruct, USERSPACE_F055D51FBF78DB84::StructWithStruct, USERSPACE_F055D51FBF78DB84::OtherTypes>>, EVOLUTOR> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_F055D51FBF78DB84, CHECK>::value>>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_F055D51FBF78DB84::SimpleStruct, USERSPACE_F055D51FBF78DB84::StructWithStruct, USERSPACE_F055D51FBF78DB84::OtherTypes>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(USERSPACE_F055D51FBF78DB84_Variant_B_SimpleStruct_StructWithStruct_OtherTypes_E_Cases<decltype(into), FROM, INTO, EVOLUTOR>(into));
  }
};
#endif

}  // namespace current::type_evolution
}  // namespace current

#if 0  // Boilerplate evolutors.

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_F055D51FBF78DB84, SimpleStruct, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_F055D51FBF78DB84, CustomDestinationNamespace, from.x, into.x);
  CURRENT_NATURAL_EVOLVE(USERSPACE_F055D51FBF78DB84, CustomDestinationNamespace, from.y, into.y);
  CURRENT_NATURAL_EVOLVE(USERSPACE_F055D51FBF78DB84, CustomDestinationNamespace, from.z, into.z);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_F055D51FBF78DB84, StructWithStruct, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_F055D51FBF78DB84, CustomDestinationNamespace, from.s, into.s);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_F055D51FBF78DB84, OtherTypes, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_F055D51FBF78DB84, CustomDestinationNamespace, from.enum_class, into.enum_class);
  CURRENT_NATURAL_EVOLVE(USERSPACE_F055D51FBF78DB84, CustomDestinationNamespace, from.optional, into.optional);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_F055D51FBF78DB84, StructWithVariant, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_F055D51FBF78DB84, CustomDestinationNamespace, from.v, into.v);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_F055D51FBF78DB84, Name, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_F055D51FBF78DB84, CustomDestinationNamespace, from.first, into.first);
  CURRENT_NATURAL_EVOLVE(USERSPACE_F055D51FBF78DB84, CustomDestinationNamespace, from.last, into.last);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, USERSPACE_F055D51FBF78DB84, StructWithVectorOfNames, {
  CURRENT_NATURAL_EVOLVE(USERSPACE_F055D51FBF78DB84, CustomDestinationNamespace, from.w, into.w);
});

CURRENT_TYPE_EVOLUTOR_VARIANT(CustomEvolutor, USERSPACE_F055D51FBF78DB84, Variant_B_SimpleStruct_StructWithStruct_OtherTypes_E, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(SimpleStruct, CURRENT_NATURAL_EVOLVE(USERSPACE_F055D51FBF78DB84, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(StructWithStruct, CURRENT_NATURAL_EVOLVE(USERSPACE_F055D51FBF78DB84, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(OtherTypes, CURRENT_NATURAL_EVOLVE(USERSPACE_F055D51FBF78DB84, CustomDestinationNamespace, from, into));
};

#endif  // Boilerplate evolutors.

// clang-format on

#endif  // CURRENT_USERSPACE_F055D51FBF78DB84
