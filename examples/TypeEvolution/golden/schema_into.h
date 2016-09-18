// The `current.h` file is the one from `https://github.com/C5T/Current`.
// Compile with `-std=c++11` or higher.

#include "current.h"

// clang-format off

namespace current_userspace {

#ifndef CURRENT_SCHEMA_FOR_T9203341832538601265
#define CURRENT_SCHEMA_FOR_T9203341832538601265
namespace t9203341832538601265 {
CURRENT_STRUCT(Basic) {
  CURRENT_FIELD(i, int32_t);
  CURRENT_FIELD(s, std::string);
  CURRENT_FIELD(t, std::chrono::microseconds);
};
}  // namespace t9203341832538601265
#endif  // CURRENT_SCHEMA_FOR_T_9203341832538601265

#ifndef CURRENT_SCHEMA_FOR_T9202391653942970634
#define CURRENT_SCHEMA_FOR_T9202391653942970634
namespace t9202391653942970634 {
CURRENT_STRUCT(FullName) {
  CURRENT_FIELD(full_name, std::string);
};
}  // namespace t9202391653942970634
#endif  // CURRENT_SCHEMA_FOR_T_9202391653942970634

#ifndef CURRENT_SCHEMA_FOR_T9207175600672737443
#define CURRENT_SCHEMA_FOR_T9207175600672737443
namespace t9207175600672737443 {
CURRENT_STRUCT(WithOptional) {
  CURRENT_FIELD(maybe_name, Optional<t9202391653942970634::FullName>);
};
}  // namespace t9207175600672737443
#endif  // CURRENT_SCHEMA_FOR_T_9207175600672737443

#ifndef CURRENT_SCHEMA_FOR_T9206911750362052937
#define CURRENT_SCHEMA_FOR_T9206911750362052937
namespace t9206911750362052937 {
CURRENT_STRUCT(CustomTypeA) {
  CURRENT_FIELD(a, int32_t);
};
}  // namespace t9206911750362052937
#endif  // CURRENT_SCHEMA_FOR_T_9206911750362052937

#ifndef CURRENT_SCHEMA_FOR_T9202573820625447155
#define CURRENT_SCHEMA_FOR_T9202573820625447155
namespace t9202573820625447155 {
CURRENT_STRUCT(CustomTypeB) {
  CURRENT_FIELD(b, int32_t);
};
}  // namespace t9202573820625447155
#endif  // CURRENT_SCHEMA_FOR_T_9202573820625447155

#ifndef CURRENT_SCHEMA_FOR_T9207934621170686053
#define CURRENT_SCHEMA_FOR_T9207934621170686053
namespace t9207934621170686053 {
CURRENT_STRUCT(CustomTypeC) {
  CURRENT_FIELD(c, int32_t);
};
}  // namespace t9207934621170686053
#endif  // CURRENT_SCHEMA_FOR_T_9207934621170686053

#ifndef CURRENT_SCHEMA_FOR_T9226317598374623672
#define CURRENT_SCHEMA_FOR_T9226317598374623672
namespace t9226317598374623672 {
CURRENT_VARIANT(ExpandingVariant, t9206911750362052937::CustomTypeA, t9202573820625447155::CustomTypeB, t9207934621170686053::CustomTypeC);
}  // namespace t9226317598374623672
#endif  // CURRENT_SCHEMA_FOR_T_9226317598374623672

#ifndef CURRENT_SCHEMA_FOR_T9205232737393522834
#define CURRENT_SCHEMA_FOR_T9205232737393522834
namespace t9205232737393522834 {
CURRENT_STRUCT(WithExpandingVariant) {
  CURRENT_FIELD(v, t9226317598374623672::ExpandingVariant);
};
}  // namespace t9205232737393522834
#endif  // CURRENT_SCHEMA_FOR_T_9205232737393522834

#ifndef CURRENT_SCHEMA_FOR_T9221067730773882392
#define CURRENT_SCHEMA_FOR_T9221067730773882392
namespace t9221067730773882392 {
CURRENT_VARIANT(ShrinkingVariant, t9206911750362052937::CustomTypeA, t9202573820625447155::CustomTypeB);
}  // namespace t9221067730773882392
#endif  // CURRENT_SCHEMA_FOR_T_9221067730773882392

#ifndef CURRENT_SCHEMA_FOR_T9200848076931525722
#define CURRENT_SCHEMA_FOR_T9200848076931525722
namespace t9200848076931525722 {
CURRENT_STRUCT(WithShrinkingVariant) {
  CURRENT_FIELD(v, t9221067730773882392::ShrinkingVariant);
};
}  // namespace t9200848076931525722
#endif  // CURRENT_SCHEMA_FOR_T_9200848076931525722

#ifndef CURRENT_SCHEMA_FOR_T9207419971064567476
#define CURRENT_SCHEMA_FOR_T9207419971064567476
namespace t9207419971064567476 {
CURRENT_STRUCT(WithFieldsToRemove) {
  CURRENT_FIELD(foo, std::string);
  CURRENT_FIELD(bar, std::string);
};
}  // namespace t9207419971064567476
#endif  // CURRENT_SCHEMA_FOR_T_9207419971064567476

#ifndef CURRENT_SCHEMA_FOR_T9222603216121463524
#define CURRENT_SCHEMA_FOR_T9222603216121463524
namespace t9222603216121463524 {
CURRENT_VARIANT(All, t9203341832538601265::Basic, t9202391653942970634::FullName, t9207175600672737443::WithOptional, t9205232737393522834::WithExpandingVariant, t9200848076931525722::WithShrinkingVariant, t9207419971064567476::WithFieldsToRemove);
}  // namespace t9222603216121463524
#endif  // CURRENT_SCHEMA_FOR_T_9222603216121463524

#ifndef CURRENT_SCHEMA_FOR_T9207746704185948264
#define CURRENT_SCHEMA_FOR_T9207746704185948264
namespace t9207746704185948264 {
CURRENT_STRUCT(TopLevel) {
  CURRENT_FIELD(data, t9222603216121463524::All);
};
}  // namespace t9207746704185948264
#endif  // CURRENT_SCHEMA_FOR_T_9207746704185948264

}  // namespace current_userspace

#ifndef CURRENT_NAMESPACE_SchemaInto_DEFINED
#define CURRENT_NAMESPACE_SchemaInto_DEFINED
CURRENT_NAMESPACE(SchemaInto) {
  CURRENT_NAMESPACE_TYPE(WithShrinkingVariant, current_userspace::t9200848076931525722::WithShrinkingVariant);
  CURRENT_NAMESPACE_TYPE(FullName, current_userspace::t9202391653942970634::FullName);
  CURRENT_NAMESPACE_TYPE(CustomTypeB, current_userspace::t9202573820625447155::CustomTypeB);
  CURRENT_NAMESPACE_TYPE(Basic, current_userspace::t9203341832538601265::Basic);
  CURRENT_NAMESPACE_TYPE(WithExpandingVariant, current_userspace::t9205232737393522834::WithExpandingVariant);
  CURRENT_NAMESPACE_TYPE(CustomTypeA, current_userspace::t9206911750362052937::CustomTypeA);
  CURRENT_NAMESPACE_TYPE(WithOptional, current_userspace::t9207175600672737443::WithOptional);
  CURRENT_NAMESPACE_TYPE(WithFieldsToRemove, current_userspace::t9207419971064567476::WithFieldsToRemove);
  CURRENT_NAMESPACE_TYPE(TopLevel, current_userspace::t9207746704185948264::TopLevel);
  CURRENT_NAMESPACE_TYPE(CustomTypeC, current_userspace::t9207934621170686053::CustomTypeC);
  CURRENT_NAMESPACE_TYPE(ShrinkingVariant, current_userspace::t9221067730773882392::ShrinkingVariant);
  CURRENT_NAMESPACE_TYPE(All, current_userspace::t9222603216121463524::All);
  CURRENT_NAMESPACE_TYPE(ExpandingVariant, current_userspace::t9226317598374623672::ExpandingVariant);

  // Privileged types.
  CURRENT_NAMESPACE_TYPE(ExposedTopLevel, current_userspace::t9207746704185948264::TopLevel);
};  // CURRENT_NAMESPACE(SchemaInto)
#endif  // CURRENT_NAMESPACE_SchemaInto_DEFINED

namespace current {
namespace type_evolution {

// Default evolution for struct `WithShrinkingVariant`.
#ifndef DEFAULT_EVOLUTION_73347CD680D1CCEAE5C5A43A1AD435991DD16AAC6E89BD44219F7128735A751B  // typename SchemaInto::WithShrinkingVariant
#define DEFAULT_EVOLUTION_73347CD680D1CCEAE5C5A43A1AD435991DD16AAC6E89BD44219F7128735A751B  // typename SchemaInto::WithShrinkingVariant
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaInto, typename SchemaInto::WithShrinkingVariant, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaInto;
  template <typename INTO>
  static void Go(const typename FROM::WithShrinkingVariant& from,
                 typename INTO::WithShrinkingVariant& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::WithShrinkingVariant>::value == 1,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(v);
  }
};
#endif

// Default evolution for struct `FullName`.
#ifndef DEFAULT_EVOLUTION_19E611AF840FB7BDEFED1FF83FE4F5AC306F35E55FD643D1E4A3B0B8856E0CE7  // typename SchemaInto::FullName
#define DEFAULT_EVOLUTION_19E611AF840FB7BDEFED1FF83FE4F5AC306F35E55FD643D1E4A3B0B8856E0CE7  // typename SchemaInto::FullName
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaInto, typename SchemaInto::FullName, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaInto;
  template <typename INTO>
  static void Go(const typename FROM::FullName& from,
                 typename INTO::FullName& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::FullName>::value == 1,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(full_name);
  }
};
#endif

// Default evolution for struct `CustomTypeB`.
#ifndef DEFAULT_EVOLUTION_401263DCABE0E0ED22AAF094D03B57F7BBD00A7B151E5DD0C6A277348EF22EBC  // typename SchemaInto::CustomTypeB
#define DEFAULT_EVOLUTION_401263DCABE0E0ED22AAF094D03B57F7BBD00A7B151E5DD0C6A277348EF22EBC  // typename SchemaInto::CustomTypeB
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaInto, typename SchemaInto::CustomTypeB, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaInto;
  template <typename INTO>
  static void Go(const typename FROM::CustomTypeB& from,
                 typename INTO::CustomTypeB& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::CustomTypeB>::value == 1,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(b);
  }
};
#endif

// Default evolution for struct `Basic`.
#ifndef DEFAULT_EVOLUTION_49CE7C9364776782B9E710C132A740B65EB6E131E29FADD84F41F363CE9EDCF7  // typename SchemaInto::Basic
#define DEFAULT_EVOLUTION_49CE7C9364776782B9E710C132A740B65EB6E131E29FADD84F41F363CE9EDCF7  // typename SchemaInto::Basic
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaInto, typename SchemaInto::Basic, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaInto;
  template <typename INTO>
  static void Go(const typename FROM::Basic& from,
                 typename INTO::Basic& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Basic>::value == 3,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(i);
      CURRENT_EVOLVE_FIELD(s);
      CURRENT_EVOLVE_FIELD(t);
  }
};
#endif

// Default evolution for struct `WithExpandingVariant`.
#ifndef DEFAULT_EVOLUTION_F40BD3608F1AAF051B002A736F2100B9BCA817B2DAF5F99E309801643853C5AA  // typename SchemaInto::WithExpandingVariant
#define DEFAULT_EVOLUTION_F40BD3608F1AAF051B002A736F2100B9BCA817B2DAF5F99E309801643853C5AA  // typename SchemaInto::WithExpandingVariant
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaInto, typename SchemaInto::WithExpandingVariant, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaInto;
  template <typename INTO>
  static void Go(const typename FROM::WithExpandingVariant& from,
                 typename INTO::WithExpandingVariant& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::WithExpandingVariant>::value == 1,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(v);
  }
};
#endif

// Default evolution for struct `CustomTypeA`.
#ifndef DEFAULT_EVOLUTION_BC7B69EABBD6AE5BC015566237CB74AABA47FEA4E27006DA6FC005AF79A190B4  // typename SchemaInto::CustomTypeA
#define DEFAULT_EVOLUTION_BC7B69EABBD6AE5BC015566237CB74AABA47FEA4E27006DA6FC005AF79A190B4  // typename SchemaInto::CustomTypeA
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaInto, typename SchemaInto::CustomTypeA, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaInto;
  template <typename INTO>
  static void Go(const typename FROM::CustomTypeA& from,
                 typename INTO::CustomTypeA& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::CustomTypeA>::value == 1,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(a);
  }
};
#endif

// Default evolution for struct `WithOptional`.
#ifndef DEFAULT_EVOLUTION_0441EE9186F02A56A0C3AE15C1E07E14DDD92B7E03F6FFE99607F803A259C768  // typename SchemaInto::WithOptional
#define DEFAULT_EVOLUTION_0441EE9186F02A56A0C3AE15C1E07E14DDD92B7E03F6FFE99607F803A259C768  // typename SchemaInto::WithOptional
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaInto, typename SchemaInto::WithOptional, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaInto;
  template <typename INTO>
  static void Go(const typename FROM::WithOptional& from,
                 typename INTO::WithOptional& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::WithOptional>::value == 1,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(maybe_name);
  }
};
#endif

// Default evolution for struct `WithFieldsToRemove`.
#ifndef DEFAULT_EVOLUTION_3AC4D4D252207B1BE5793B4CED5C3622BEA3C07DC7DB117413B8F15F7EBE200B  // typename SchemaInto::WithFieldsToRemove
#define DEFAULT_EVOLUTION_3AC4D4D252207B1BE5793B4CED5C3622BEA3C07DC7DB117413B8F15F7EBE200B  // typename SchemaInto::WithFieldsToRemove
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaInto, typename SchemaInto::WithFieldsToRemove, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaInto;
  template <typename INTO>
  static void Go(const typename FROM::WithFieldsToRemove& from,
                 typename INTO::WithFieldsToRemove& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::WithFieldsToRemove>::value == 2,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(foo);
      CURRENT_EVOLVE_FIELD(bar);
  }
};
#endif

// Default evolution for struct `TopLevel`.
#ifndef DEFAULT_EVOLUTION_F57A2CAED90441540FAD6B34604A9F09AF35682CAE986F83A5F7163815F17B58  // typename SchemaInto::TopLevel
#define DEFAULT_EVOLUTION_F57A2CAED90441540FAD6B34604A9F09AF35682CAE986F83A5F7163815F17B58  // typename SchemaInto::TopLevel
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaInto, typename SchemaInto::TopLevel, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaInto;
  template <typename INTO>
  static void Go(const typename FROM::TopLevel& from,
                 typename INTO::TopLevel& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TopLevel>::value == 1,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(data);
  }
};
#endif

// Default evolution for struct `CustomTypeC`.
#ifndef DEFAULT_EVOLUTION_C572DFD75AEC86F64C00F25FBBE3335F5A7F082A6ECAC30C9AC1E6498DF6A65E  // typename SchemaInto::CustomTypeC
#define DEFAULT_EVOLUTION_C572DFD75AEC86F64C00F25FBBE3335F5A7F082A6ECAC30C9AC1E6498DF6A65E  // typename SchemaInto::CustomTypeC
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaInto, typename SchemaInto::CustomTypeC, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaInto;
  template <typename INTO>
  static void Go(const typename FROM::CustomTypeC& from,
                 typename INTO::CustomTypeC& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::CustomTypeC>::value == 1,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(c);
  }
};
#endif

// Default evolution for `Optional<t9202391653942970634::FullName>`.
#ifndef DEFAULT_EVOLUTION_4DEA62842F0DCC86C780454FA0613F81168E6F0D83B7AF9F6CEB4BA3B52A47FD  // Optional<typename SchemaInto::FullName>
#define DEFAULT_EVOLUTION_4DEA62842F0DCC86C780454FA0613F81168E6F0D83B7AF9F6CEB4BA3B52A47FD  // Optional<typename SchemaInto::FullName>
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaInto, Optional<typename SchemaInto::FullName>, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO, typename INTO_TYPE>
  static void Go(const Optional<typename SchemaInto::FullName>& from, INTO_TYPE& into) {
    if (Exists(from)) {
      typename INTO::FullName evolved;
      Evolve<SchemaInto, typename SchemaInto::FullName, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(Value(from), evolved);
      into = evolved;
    } else {
      into = nullptr;
    }
  }
};
#endif

// Default evolution for `Variant<CustomTypeA, CustomTypeB>`.
#ifndef DEFAULT_EVOLUTION_A5F97C85D92C29285CC44882822F465CE06EC4E604964DD1331BEB8C88E029AF  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaInto::CustomTypeA, SchemaInto::CustomTypeB>>
#define DEFAULT_EVOLUTION_A5F97C85D92C29285CC44882822F465CE06EC4E604964DD1331BEB8C88E029AF  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaInto::CustomTypeA, SchemaInto::CustomTypeB>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename CURRENT_ACTIVE_EVOLVER>
struct SchemaInto_ShrinkingVariant_Cases {
  DST& into;
  explicit SchemaInto_ShrinkingVariant_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::CustomTypeA& value) const {
    using into_t = typename INTO::CustomTypeA;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::CustomTypeA, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::CustomTypeB& value) const {
    using into_t = typename INTO::CustomTypeB;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::CustomTypeB, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename CURRENT_ACTIVE_EVOLVER, typename VARIANT_NAME_HELPER>
struct Evolve<SchemaInto, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaInto::CustomTypeA, SchemaInto::CustomTypeB>>, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaInto::CustomTypeA, SchemaInto::CustomTypeB>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(SchemaInto_ShrinkingVariant_Cases<decltype(into), SchemaInto, INTO, CURRENT_ACTIVE_EVOLVER>(into));
  }
};
#endif

// Default evolution for `Variant<Basic, FullName, WithOptional, WithExpandingVariant, WithShrinkingVariant, WithFieldsToRemove>`.
#ifndef DEFAULT_EVOLUTION_1DF45248328910AAB44B94E4A0FD1504E6566CE97C044141BC9F8A9AD5A99373  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaInto::Basic, SchemaInto::FullName, SchemaInto::WithOptional, SchemaInto::WithExpandingVariant, SchemaInto::WithShrinkingVariant, SchemaInto::WithFieldsToRemove>>
#define DEFAULT_EVOLUTION_1DF45248328910AAB44B94E4A0FD1504E6566CE97C044141BC9F8A9AD5A99373  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaInto::Basic, SchemaInto::FullName, SchemaInto::WithOptional, SchemaInto::WithExpandingVariant, SchemaInto::WithShrinkingVariant, SchemaInto::WithFieldsToRemove>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename CURRENT_ACTIVE_EVOLVER>
struct SchemaInto_All_Cases {
  DST& into;
  explicit SchemaInto_All_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::Basic& value) const {
    using into_t = typename INTO::Basic;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::Basic, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::FullName& value) const {
    using into_t = typename INTO::FullName;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::FullName, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::WithOptional& value) const {
    using into_t = typename INTO::WithOptional;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::WithOptional, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::WithExpandingVariant& value) const {
    using into_t = typename INTO::WithExpandingVariant;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::WithExpandingVariant, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::WithShrinkingVariant& value) const {
    using into_t = typename INTO::WithShrinkingVariant;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::WithShrinkingVariant, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::WithFieldsToRemove& value) const {
    using into_t = typename INTO::WithFieldsToRemove;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::WithFieldsToRemove, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename CURRENT_ACTIVE_EVOLVER, typename VARIANT_NAME_HELPER>
struct Evolve<SchemaInto, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaInto::Basic, SchemaInto::FullName, SchemaInto::WithOptional, SchemaInto::WithExpandingVariant, SchemaInto::WithShrinkingVariant, SchemaInto::WithFieldsToRemove>>, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaInto::Basic, SchemaInto::FullName, SchemaInto::WithOptional, SchemaInto::WithExpandingVariant, SchemaInto::WithShrinkingVariant, SchemaInto::WithFieldsToRemove>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(SchemaInto_All_Cases<decltype(into), SchemaInto, INTO, CURRENT_ACTIVE_EVOLVER>(into));
  }
};
#endif

// Default evolution for `Variant<CustomTypeA, CustomTypeB, CustomTypeC>`.
#ifndef DEFAULT_EVOLUTION_442F1936A72B72E3F458B701894008340E60D7038A05A42E74CCDC72FDD39371  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaInto::CustomTypeA, SchemaInto::CustomTypeB, SchemaInto::CustomTypeC>>
#define DEFAULT_EVOLUTION_442F1936A72B72E3F458B701894008340E60D7038A05A42E74CCDC72FDD39371  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaInto::CustomTypeA, SchemaInto::CustomTypeB, SchemaInto::CustomTypeC>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename CURRENT_ACTIVE_EVOLVER>
struct SchemaInto_ExpandingVariant_Cases {
  DST& into;
  explicit SchemaInto_ExpandingVariant_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::CustomTypeA& value) const {
    using into_t = typename INTO::CustomTypeA;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::CustomTypeA, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::CustomTypeB& value) const {
    using into_t = typename INTO::CustomTypeB;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::CustomTypeB, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::CustomTypeC& value) const {
    using into_t = typename INTO::CustomTypeC;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::CustomTypeC, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename CURRENT_ACTIVE_EVOLVER, typename VARIANT_NAME_HELPER>
struct Evolve<SchemaInto, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaInto::CustomTypeA, SchemaInto::CustomTypeB, SchemaInto::CustomTypeC>>, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaInto::CustomTypeA, SchemaInto::CustomTypeB, SchemaInto::CustomTypeC>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(SchemaInto_ExpandingVariant_Cases<decltype(into), SchemaInto, INTO, CURRENT_ACTIVE_EVOLVER>(into));
  }
};
#endif

}  // namespace current::type_evolution
}  // namespace current

#if 0  // Boilerplate evolvers.

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaInto, WithShrinkingVariant, {
  CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from.v, into.v);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaInto, FullName, {
  CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from.full_name, into.full_name);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaInto, CustomTypeB, {
  CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from.b, into.b);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaInto, Basic, {
  CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from.i, into.i);
  CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from.s, into.s);
  CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from.t, into.t);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaInto, WithExpandingVariant, {
  CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from.v, into.v);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaInto, CustomTypeA, {
  CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from.a, into.a);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaInto, WithOptional, {
  CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from.maybe_name, into.maybe_name);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaInto, WithFieldsToRemove, {
  CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from.foo, into.foo);
  CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from.bar, into.bar);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaInto, TopLevel, {
  CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from.data, into.data);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaInto, CustomTypeC, {
  CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from.c, into.c);
});

CURRENT_TYPE_EVOLVER_VARIANT(CustomEvolver, SchemaInto, t9221067730773882392::ShrinkingVariant, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9206911750362052937::CustomTypeA, CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9202573820625447155::CustomTypeB, CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from, into));
};

CURRENT_TYPE_EVOLVER_VARIANT(CustomEvolver, SchemaInto, t9222603216121463524::All, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9203341832538601265::Basic, CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9202391653942970634::FullName, CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9207175600672737443::WithOptional, CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9205232737393522834::WithExpandingVariant, CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9200848076931525722::WithShrinkingVariant, CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9207419971064567476::WithFieldsToRemove, CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from, into));
};

CURRENT_TYPE_EVOLVER_VARIANT(CustomEvolver, SchemaInto, t9226317598374623672::ExpandingVariant, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9206911750362052937::CustomTypeA, CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9202573820625447155::CustomTypeB, CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9207934621170686053::CustomTypeC, CURRENT_NATURAL_EVOLVE(SchemaInto, CustomDestinationNamespace, from, into));
};

#endif  // Boilerplate evolvers.

// clang-format on
