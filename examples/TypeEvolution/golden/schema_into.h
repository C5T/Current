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

#ifndef CURRENT_NAMESPACE_Into_DEFINED
#define CURRENT_NAMESPACE_Into_DEFINED
CURRENT_NAMESPACE(Into) {
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
};  // CURRENT_NAMESPACE(Into)
#endif  // CURRENT_NAMESPACE_Into_DEFINED

namespace current {
namespace type_evolution {

// Default evolution for struct `WithShrinkingVariant`.
#ifndef DEFAULT_EVOLUTION_635FF9A23574EDEA2DEF09F40EAF99BD88B386FF64CC397D9A15A22B2FD4F180  // typename Into::WithShrinkingVariant
#define DEFAULT_EVOLUTION_635FF9A23574EDEA2DEF09F40EAF99BD88B386FF64CC397D9A15A22B2FD4F180  // typename Into::WithShrinkingVariant
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<Into, typename Into::WithShrinkingVariant, CURRENT_ACTIVE_EVOLVER> {
  using FROM = Into;
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
#ifndef DEFAULT_EVOLUTION_74FE7CF20A7A7F7F0EB974896447590D0C82A4437D12532D6258CB796F7DDEF7  // typename Into::FullName
#define DEFAULT_EVOLUTION_74FE7CF20A7A7F7F0EB974896447590D0C82A4437D12532D6258CB796F7DDEF7  // typename Into::FullName
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<Into, typename Into::FullName, CURRENT_ACTIVE_EVOLVER> {
  using FROM = Into;
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
#ifndef DEFAULT_EVOLUTION_C66FCB4CAB0D4C774A62F422F479E09ED963E8C9AA6D7078DADB4D499FEE3AF3  // typename Into::CustomTypeB
#define DEFAULT_EVOLUTION_C66FCB4CAB0D4C774A62F422F479E09ED963E8C9AA6D7078DADB4D499FEE3AF3  // typename Into::CustomTypeB
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<Into, typename Into::CustomTypeB, CURRENT_ACTIVE_EVOLVER> {
  using FROM = Into;
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
#ifndef DEFAULT_EVOLUTION_997FD215786594F423913E7905EED459F1845943406A4806937E61BFB2483354  // typename Into::Basic
#define DEFAULT_EVOLUTION_997FD215786594F423913E7905EED459F1845943406A4806937E61BFB2483354  // typename Into::Basic
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<Into, typename Into::Basic, CURRENT_ACTIVE_EVOLVER> {
  using FROM = Into;
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
#ifndef DEFAULT_EVOLUTION_376898126CF51B0170AF638DAE5AC149137BF6C6EBFF392039AFFB8DD46F01AC  // typename Into::WithExpandingVariant
#define DEFAULT_EVOLUTION_376898126CF51B0170AF638DAE5AC149137BF6C6EBFF392039AFFB8DD46F01AC  // typename Into::WithExpandingVariant
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<Into, typename Into::WithExpandingVariant, CURRENT_ACTIVE_EVOLVER> {
  using FROM = Into;
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
#ifndef DEFAULT_EVOLUTION_FB3D460185CDCFDA13F3191B73A2EF824E182246CAAF0A9B49DD96043ED1EAA3  // typename Into::CustomTypeA
#define DEFAULT_EVOLUTION_FB3D460185CDCFDA13F3191B73A2EF824E182246CAAF0A9B49DD96043ED1EAA3  // typename Into::CustomTypeA
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<Into, typename Into::CustomTypeA, CURRENT_ACTIVE_EVOLVER> {
  using FROM = Into;
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
#ifndef DEFAULT_EVOLUTION_0B47BEC7EDE0F59DB04F09DE30BDE1FC15B8EE275FA32270D8C1950C0D8496E6  // typename Into::WithOptional
#define DEFAULT_EVOLUTION_0B47BEC7EDE0F59DB04F09DE30BDE1FC15B8EE275FA32270D8C1950C0D8496E6  // typename Into::WithOptional
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<Into, typename Into::WithOptional, CURRENT_ACTIVE_EVOLVER> {
  using FROM = Into;
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
#ifndef DEFAULT_EVOLUTION_153A8A367106BC172A11928CF5272FDFEC03C3E8E0AB59D9DFC00159F9E5868B  // typename Into::WithFieldsToRemove
#define DEFAULT_EVOLUTION_153A8A367106BC172A11928CF5272FDFEC03C3E8E0AB59D9DFC00159F9E5868B  // typename Into::WithFieldsToRemove
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<Into, typename Into::WithFieldsToRemove, CURRENT_ACTIVE_EVOLVER> {
  using FROM = Into;
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
#ifndef DEFAULT_EVOLUTION_2DBADA8115FE0D6209C301462726C0D52E0EE23011704F80BEB546C9D07BADE0  // typename Into::TopLevel
#define DEFAULT_EVOLUTION_2DBADA8115FE0D6209C301462726C0D52E0EE23011704F80BEB546C9D07BADE0  // typename Into::TopLevel
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<Into, typename Into::TopLevel, CURRENT_ACTIVE_EVOLVER> {
  using FROM = Into;
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
#ifndef DEFAULT_EVOLUTION_9070DCC9D25F4231B14503FFADE377378132575004ED756199548455B4014510  // typename Into::CustomTypeC
#define DEFAULT_EVOLUTION_9070DCC9D25F4231B14503FFADE377378132575004ED756199548455B4014510  // typename Into::CustomTypeC
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<Into, typename Into::CustomTypeC, CURRENT_ACTIVE_EVOLVER> {
  using FROM = Into;
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
#ifndef DEFAULT_EVOLUTION_DA80F3438EB3560F915ACDFE903A552D0852EB80F85F931CE5A1B951F2028158  // Optional<typename Into::FullName>
#define DEFAULT_EVOLUTION_DA80F3438EB3560F915ACDFE903A552D0852EB80F85F931CE5A1B951F2028158  // Optional<typename Into::FullName>
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<Into, Optional<typename Into::FullName>, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO, typename INTO_TYPE>
  static void Go(const Optional<typename Into::FullName>& from, INTO_TYPE& into) {
    if (Exists(from)) {
      typename INTO::FullName evolved;
      Evolve<Into, typename Into::FullName, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(Value(from), evolved);
      into = evolved;
    } else {
      into = nullptr;
    }
  }
};
#endif

// Default evolution for `Variant<CustomTypeA, CustomTypeB>`.
#ifndef DEFAULT_EVOLUTION_9050E9A9E57CAC9E8F8676D544F15E9B6D4D468433695A402F2BBCC7C1A00C57  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<Into::CustomTypeA, Into::CustomTypeB>>
#define DEFAULT_EVOLUTION_9050E9A9E57CAC9E8F8676D544F15E9B6D4D468433695A402F2BBCC7C1A00C57  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<Into::CustomTypeA, Into::CustomTypeB>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename CURRENT_ACTIVE_EVOLVER>
struct Into_ShrinkingVariant_Cases {
  DST& into;
  explicit Into_ShrinkingVariant_Cases(DST& into) : into(into) {}
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
struct Evolve<Into, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<Into::CustomTypeA, Into::CustomTypeB>>, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<Into::CustomTypeA, Into::CustomTypeB>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(Into_ShrinkingVariant_Cases<decltype(into), Into, INTO, CURRENT_ACTIVE_EVOLVER>(into));
  }
};
#endif

// Default evolution for `Variant<Basic, FullName, WithOptional, WithExpandingVariant, WithShrinkingVariant, WithFieldsToRemove>`.
#ifndef DEFAULT_EVOLUTION_504EFB874A2A123E4820ACBED5DE74B6158C99BE7D214A95787AF00EF2992AC0  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<Into::Basic, Into::FullName, Into::WithOptional, Into::WithExpandingVariant, Into::WithShrinkingVariant, Into::WithFieldsToRemove>>
#define DEFAULT_EVOLUTION_504EFB874A2A123E4820ACBED5DE74B6158C99BE7D214A95787AF00EF2992AC0  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<Into::Basic, Into::FullName, Into::WithOptional, Into::WithExpandingVariant, Into::WithShrinkingVariant, Into::WithFieldsToRemove>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename CURRENT_ACTIVE_EVOLVER>
struct Into_All_Cases {
  DST& into;
  explicit Into_All_Cases(DST& into) : into(into) {}
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
struct Evolve<Into, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<Into::Basic, Into::FullName, Into::WithOptional, Into::WithExpandingVariant, Into::WithShrinkingVariant, Into::WithFieldsToRemove>>, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<Into::Basic, Into::FullName, Into::WithOptional, Into::WithExpandingVariant, Into::WithShrinkingVariant, Into::WithFieldsToRemove>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(Into_All_Cases<decltype(into), Into, INTO, CURRENT_ACTIVE_EVOLVER>(into));
  }
};
#endif

// Default evolution for `Variant<CustomTypeA, CustomTypeB, CustomTypeC>`.
#ifndef DEFAULT_EVOLUTION_5CAAF0D590810736648810CFCE341690D406E7CCB0EB6E894957768A0AEA8732  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<Into::CustomTypeA, Into::CustomTypeB, Into::CustomTypeC>>
#define DEFAULT_EVOLUTION_5CAAF0D590810736648810CFCE341690D406E7CCB0EB6E894957768A0AEA8732  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<Into::CustomTypeA, Into::CustomTypeB, Into::CustomTypeC>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename CURRENT_ACTIVE_EVOLVER>
struct Into_ExpandingVariant_Cases {
  DST& into;
  explicit Into_ExpandingVariant_Cases(DST& into) : into(into) {}
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
struct Evolve<Into, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<Into::CustomTypeA, Into::CustomTypeB, Into::CustomTypeC>>, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<Into::CustomTypeA, Into::CustomTypeB, Into::CustomTypeC>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(Into_ExpandingVariant_Cases<decltype(into), Into, INTO, CURRENT_ACTIVE_EVOLVER>(into));
  }
};
#endif

}  // namespace current::type_evolution
}  // namespace current

#if 0  // Boilerplate evolvers.

CURRENT_TYPE_EVOLVER(CustomEvolver, Into, WithShrinkingVariant, {
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.v, into.v);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, Into, FullName, {
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.full_name, into.full_name);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, Into, CustomTypeB, {
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.b, into.b);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, Into, Basic, {
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.i, into.i);
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.s, into.s);
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.t, into.t);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, Into, WithExpandingVariant, {
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.v, into.v);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, Into, CustomTypeA, {
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.a, into.a);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, Into, WithOptional, {
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.maybe_name, into.maybe_name);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, Into, WithFieldsToRemove, {
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.foo, into.foo);
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.bar, into.bar);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, Into, TopLevel, {
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.data, into.data);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, Into, CustomTypeC, {
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.c, into.c);
});

CURRENT_TYPE_EVOLVER_VARIANT(CustomEvolver, Into, t9221067730773882392::ShrinkingVariant, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9206911750362052937::CustomTypeA, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9202573820625447155::CustomTypeB, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
};

CURRENT_TYPE_EVOLVER_VARIANT(CustomEvolver, Into, t9222603216121463524::All, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9203341832538601265::Basic, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9202391653942970634::FullName, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9207175600672737443::WithOptional, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9205232737393522834::WithExpandingVariant, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9200848076931525722::WithShrinkingVariant, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9207419971064567476::WithFieldsToRemove, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
};

CURRENT_TYPE_EVOLVER_VARIANT(CustomEvolver, Into, t9226317598374623672::ExpandingVariant, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9206911750362052937::CustomTypeA, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9202573820625447155::CustomTypeB, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9207934621170686053::CustomTypeC, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
};

#endif  // Boilerplate evolvers.

// clang-format on
