// The `current.h` file is the one from `https://github.com/C5T/Current`.
// Compile with `-std=c++11` or higher.

#ifndef CURRENT_USERSPACE_B6BA536DC3EE17F4
#define CURRENT_USERSPACE_B6BA536DC3EE17F4

#include "current.h"

// clang-format off

namespace current_userspace_b6ba536dc3ee17f4 {

CURRENT_STRUCT(Basic) {
  CURRENT_FIELD(i, int32_t);
  CURRENT_FIELD(s, std::string);
  CURRENT_FIELD(t, std::chrono::microseconds);
};
using T9203341832538601265 = Basic;

CURRENT_STRUCT(FullName) {
  CURRENT_FIELD(first_name, std::string);
  CURRENT_FIELD(last_name, std::string);
};
using T9201952458119363219 = FullName;

CURRENT_STRUCT(WithOptional) {
  CURRENT_FIELD(maybe_name, Optional<FullName>);
};
using T9209615657629583566 = WithOptional;

CURRENT_STRUCT(CustomTypeA) {
  CURRENT_FIELD(a, int32_t);
};
using T9206911750362052937 = CustomTypeA;

CURRENT_STRUCT(CustomTypeB) {
  CURRENT_FIELD(b, int32_t);
};
using T9202573820625447155 = CustomTypeB;

CURRENT_VARIANT(ExpandingVariant, CustomTypeA, CustomTypeB);
using T9221067729821834539 = ExpandingVariant;

CURRENT_STRUCT(WithExpandingVariant) {
  CURRENT_FIELD(v, ExpandingVariant);
};
using T9201009161779005289 = WithExpandingVariant;

CURRENT_STRUCT(CustomTypeC) {
  CURRENT_FIELD(c, int32_t);
};
using T9207934621170686053 = CustomTypeC;

CURRENT_VARIANT(ShrinkingVariant, CustomTypeA, CustomTypeB, CustomTypeC);
using T9226317599863657099 = ShrinkingVariant;

CURRENT_STRUCT(WithShrinkingVariant) {
  CURRENT_FIELD(v, ShrinkingVariant);
};
using T9205425707876881313 = WithShrinkingVariant;

CURRENT_STRUCT(WithFieldsToRemove) {
  CURRENT_FIELD(foo, std::string);
  CURRENT_FIELD(bar, std::string);
  CURRENT_FIELD(baz, std::vector<std::string>);
};
using T9206820554754825223 = WithFieldsToRemove;

CURRENT_VARIANT(All, Basic, FullName, WithOptional, WithExpandingVariant, WithShrinkingVariant, WithFieldsToRemove);
using T9227074222822990824 = All;

CURRENT_STRUCT(TopLevel) {
  CURRENT_FIELD(data, All);
};
using T9204063025278945160 = TopLevel;

}  // namespace current_userspace_b6ba536dc3ee17f4

CURRENT_NAMESPACE(USERSPACE_B6BA536DC3EE17F4) {
  CURRENT_NAMESPACE_TYPE(WithExpandingVariant, current_userspace_b6ba536dc3ee17f4::WithExpandingVariant);
  CURRENT_NAMESPACE_TYPE(FullName, current_userspace_b6ba536dc3ee17f4::FullName);
  CURRENT_NAMESPACE_TYPE(CustomTypeB, current_userspace_b6ba536dc3ee17f4::CustomTypeB);
  CURRENT_NAMESPACE_TYPE(Basic, current_userspace_b6ba536dc3ee17f4::Basic);
  CURRENT_NAMESPACE_TYPE(TopLevel, current_userspace_b6ba536dc3ee17f4::TopLevel);
  CURRENT_NAMESPACE_TYPE(WithShrinkingVariant, current_userspace_b6ba536dc3ee17f4::WithShrinkingVariant);
  CURRENT_NAMESPACE_TYPE(WithFieldsToRemove, current_userspace_b6ba536dc3ee17f4::WithFieldsToRemove);
  CURRENT_NAMESPACE_TYPE(CustomTypeA, current_userspace_b6ba536dc3ee17f4::CustomTypeA);
  CURRENT_NAMESPACE_TYPE(CustomTypeC, current_userspace_b6ba536dc3ee17f4::CustomTypeC);
  CURRENT_NAMESPACE_TYPE(WithOptional, current_userspace_b6ba536dc3ee17f4::WithOptional);
  CURRENT_NAMESPACE_TYPE(ExpandingVariant, current_userspace_b6ba536dc3ee17f4::ExpandingVariant);
  CURRENT_NAMESPACE_TYPE(ShrinkingVariant, current_userspace_b6ba536dc3ee17f4::ShrinkingVariant);
  CURRENT_NAMESPACE_TYPE(All, current_userspace_b6ba536dc3ee17f4::All);
};  // CURRENT_NAMESPACE(USERSPACE_B6BA536DC3EE17F4)

namespace current {
namespace type_evolution {

// Default evolution for struct `WithExpandingVariant`.
#ifndef DEFAULT_EVOLUTION_26C542908F30CFA04223C424894523523017796481213A00B72C962E03F19A22  // typename USERSPACE_B6BA536DC3EE17F4::WithExpandingVariant
#define DEFAULT_EVOLUTION_26C542908F30CFA04223C424894523523017796481213A00B72C962E03F19A22  // typename USERSPACE_B6BA536DC3EE17F4::WithExpandingVariant
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B6BA536DC3EE17F4::WithExpandingVariant, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B6BA536DC3EE17F4, CHECK>::value>>
  static void Go(const typename USERSPACE_B6BA536DC3EE17F4::WithExpandingVariant& from,
                 typename INTO::WithExpandingVariant& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::WithExpandingVariant>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.v), EVOLUTOR>::template Go<INTO>(from.v, into.v);
  }
};
#endif

// Default evolution for struct `FullName`.
#ifndef DEFAULT_EVOLUTION_402900E12E64EFDDA32F2E44F6A0A0876ED9EC44118AE4DE73A972873CED60EB  // typename USERSPACE_B6BA536DC3EE17F4::FullName
#define DEFAULT_EVOLUTION_402900E12E64EFDDA32F2E44F6A0A0876ED9EC44118AE4DE73A972873CED60EB  // typename USERSPACE_B6BA536DC3EE17F4::FullName
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B6BA536DC3EE17F4::FullName, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B6BA536DC3EE17F4, CHECK>::value>>
  static void Go(const typename USERSPACE_B6BA536DC3EE17F4::FullName& from,
                 typename INTO::FullName& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::FullName>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.first_name), EVOLUTOR>::template Go<INTO>(from.first_name, into.first_name);
      Evolve<FROM, decltype(from.last_name), EVOLUTOR>::template Go<INTO>(from.last_name, into.last_name);
  }
};
#endif

// Default evolution for struct `CustomTypeB`.
#ifndef DEFAULT_EVOLUTION_44DF393BF5BEE8E5CCE8BC0264637EDE1016F16F27B4EED311B21AD57F014A3F  // typename USERSPACE_B6BA536DC3EE17F4::CustomTypeB
#define DEFAULT_EVOLUTION_44DF393BF5BEE8E5CCE8BC0264637EDE1016F16F27B4EED311B21AD57F014A3F  // typename USERSPACE_B6BA536DC3EE17F4::CustomTypeB
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B6BA536DC3EE17F4::CustomTypeB, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B6BA536DC3EE17F4, CHECK>::value>>
  static void Go(const typename USERSPACE_B6BA536DC3EE17F4::CustomTypeB& from,
                 typename INTO::CustomTypeB& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::CustomTypeB>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.b), EVOLUTOR>::template Go<INTO>(from.b, into.b);
  }
};
#endif

// Default evolution for struct `Basic`.
#ifndef DEFAULT_EVOLUTION_69B3006082EA508FEF9BCA5062D7C151C6B587DAB320A4F7DBB64EAC34BEF2AB  // typename USERSPACE_B6BA536DC3EE17F4::Basic
#define DEFAULT_EVOLUTION_69B3006082EA508FEF9BCA5062D7C151C6B587DAB320A4F7DBB64EAC34BEF2AB  // typename USERSPACE_B6BA536DC3EE17F4::Basic
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B6BA536DC3EE17F4::Basic, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B6BA536DC3EE17F4, CHECK>::value>>
  static void Go(const typename USERSPACE_B6BA536DC3EE17F4::Basic& from,
                 typename INTO::Basic& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Basic>::value == 3,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.i), EVOLUTOR>::template Go<INTO>(from.i, into.i);
      Evolve<FROM, decltype(from.s), EVOLUTOR>::template Go<INTO>(from.s, into.s);
      Evolve<FROM, decltype(from.t), EVOLUTOR>::template Go<INTO>(from.t, into.t);
  }
};
#endif

// Default evolution for struct `TopLevel`.
#ifndef DEFAULT_EVOLUTION_615CBD96F1465262C9A27CF52194CEC480FAEFF42736911CB805C6B028F471D4  // typename USERSPACE_B6BA536DC3EE17F4::TopLevel
#define DEFAULT_EVOLUTION_615CBD96F1465262C9A27CF52194CEC480FAEFF42736911CB805C6B028F471D4  // typename USERSPACE_B6BA536DC3EE17F4::TopLevel
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B6BA536DC3EE17F4::TopLevel, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B6BA536DC3EE17F4, CHECK>::value>>
  static void Go(const typename USERSPACE_B6BA536DC3EE17F4::TopLevel& from,
                 typename INTO::TopLevel& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TopLevel>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.data), EVOLUTOR>::template Go<INTO>(from.data, into.data);
  }
};
#endif

// Default evolution for struct `WithShrinkingVariant`.
#ifndef DEFAULT_EVOLUTION_70C44F8E791C95800DC09B728EE4609AB08DF1A99BA5D91506EF7769592F8245  // typename USERSPACE_B6BA536DC3EE17F4::WithShrinkingVariant
#define DEFAULT_EVOLUTION_70C44F8E791C95800DC09B728EE4609AB08DF1A99BA5D91506EF7769592F8245  // typename USERSPACE_B6BA536DC3EE17F4::WithShrinkingVariant
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B6BA536DC3EE17F4::WithShrinkingVariant, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B6BA536DC3EE17F4, CHECK>::value>>
  static void Go(const typename USERSPACE_B6BA536DC3EE17F4::WithShrinkingVariant& from,
                 typename INTO::WithShrinkingVariant& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::WithShrinkingVariant>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.v), EVOLUTOR>::template Go<INTO>(from.v, into.v);
  }
};
#endif

// Default evolution for struct `WithFieldsToRemove`.
#ifndef DEFAULT_EVOLUTION_CA47369564A7D7750DAE7B02C0328AF3348B3F13E13037D3715C80FFE2B090F1  // typename USERSPACE_B6BA536DC3EE17F4::WithFieldsToRemove
#define DEFAULT_EVOLUTION_CA47369564A7D7750DAE7B02C0328AF3348B3F13E13037D3715C80FFE2B090F1  // typename USERSPACE_B6BA536DC3EE17F4::WithFieldsToRemove
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B6BA536DC3EE17F4::WithFieldsToRemove, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B6BA536DC3EE17F4, CHECK>::value>>
  static void Go(const typename USERSPACE_B6BA536DC3EE17F4::WithFieldsToRemove& from,
                 typename INTO::WithFieldsToRemove& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::WithFieldsToRemove>::value == 3,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.foo), EVOLUTOR>::template Go<INTO>(from.foo, into.foo);
      Evolve<FROM, decltype(from.bar), EVOLUTOR>::template Go<INTO>(from.bar, into.bar);
      Evolve<FROM, decltype(from.baz), EVOLUTOR>::template Go<INTO>(from.baz, into.baz);
  }
};
#endif

// Default evolution for struct `CustomTypeA`.
#ifndef DEFAULT_EVOLUTION_7ADBA0113AD7E826899510B951FD2293ADC0C8E8E44B04BEF1B231FAA37BAB39  // typename USERSPACE_B6BA536DC3EE17F4::CustomTypeA
#define DEFAULT_EVOLUTION_7ADBA0113AD7E826899510B951FD2293ADC0C8E8E44B04BEF1B231FAA37BAB39  // typename USERSPACE_B6BA536DC3EE17F4::CustomTypeA
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B6BA536DC3EE17F4::CustomTypeA, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B6BA536DC3EE17F4, CHECK>::value>>
  static void Go(const typename USERSPACE_B6BA536DC3EE17F4::CustomTypeA& from,
                 typename INTO::CustomTypeA& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::CustomTypeA>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.a), EVOLUTOR>::template Go<INTO>(from.a, into.a);
  }
};
#endif

// Default evolution for struct `CustomTypeC`.
#ifndef DEFAULT_EVOLUTION_E9D4446D1CED9E385F2A0BE7F3312CE766E257F9D49970131372120F7C8A2444  // typename USERSPACE_B6BA536DC3EE17F4::CustomTypeC
#define DEFAULT_EVOLUTION_E9D4446D1CED9E385F2A0BE7F3312CE766E257F9D49970131372120F7C8A2444  // typename USERSPACE_B6BA536DC3EE17F4::CustomTypeC
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B6BA536DC3EE17F4::CustomTypeC, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B6BA536DC3EE17F4, CHECK>::value>>
  static void Go(const typename USERSPACE_B6BA536DC3EE17F4::CustomTypeC& from,
                 typename INTO::CustomTypeC& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::CustomTypeC>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.c), EVOLUTOR>::template Go<INTO>(from.c, into.c);
  }
};
#endif

// Default evolution for struct `WithOptional`.
#ifndef DEFAULT_EVOLUTION_0A5B28B3C6AEA8A350276E061547842B92AC8F3E70D017E3401448D7A9CA1CB1  // typename USERSPACE_B6BA536DC3EE17F4::WithOptional
#define DEFAULT_EVOLUTION_0A5B28B3C6AEA8A350276E061547842B92AC8F3E70D017E3401448D7A9CA1CB1  // typename USERSPACE_B6BA536DC3EE17F4::WithOptional
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B6BA536DC3EE17F4::WithOptional, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B6BA536DC3EE17F4, CHECK>::value>>
  static void Go(const typename USERSPACE_B6BA536DC3EE17F4::WithOptional& from,
                 typename INTO::WithOptional& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::WithOptional>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.maybe_name), EVOLUTOR>::template Go<INTO>(from.maybe_name, into.maybe_name);
  }
};
#endif

// Default evolution for `Optional<FullName>`.
#ifndef DEFAULT_EVOLUTION_C2A8667DF830481A17DA606FCC93CC4B0A699CE74D93B19CEFE63B1588624FAC  // Optional<typename USERSPACE_B6BA536DC3EE17F4::FullName>
#define DEFAULT_EVOLUTION_C2A8667DF830481A17DA606FCC93CC4B0A699CE74D93B19CEFE63B1588624FAC  // Optional<typename USERSPACE_B6BA536DC3EE17F4::FullName>
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, Optional<typename USERSPACE_B6BA536DC3EE17F4::FullName>, EVOLUTOR> {
  template <typename INTO, typename INTO_TYPE>
  static void Go(const Optional<typename USERSPACE_B6BA536DC3EE17F4::FullName>& from, INTO_TYPE& into) {
    if (Exists(from)) {
      typename INTO::FullName evolved;
      Evolve<FROM, typename USERSPACE_B6BA536DC3EE17F4::FullName, EVOLUTOR>::template Go<INTO>(Value(from), evolved);
      into = evolved;
    } else {
      into = nullptr;
    }
  }
};
#endif

// Default evolution for `Variant<CustomTypeA, CustomTypeB>`.
#ifndef DEFAULT_EVOLUTION_546DC178B63DCBD93314F8F15201465CF256AFABA2509C267148356B97442B33  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B6BA536DC3EE17F4::CustomTypeA, USERSPACE_B6BA536DC3EE17F4::CustomTypeB>>
#define DEFAULT_EVOLUTION_546DC178B63DCBD93314F8F15201465CF256AFABA2509C267148356B97442B33  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B6BA536DC3EE17F4::CustomTypeA, USERSPACE_B6BA536DC3EE17F4::CustomTypeB>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_B6BA536DC3EE17F4_ExpandingVariant_Cases {
  DST& into;
  explicit USERSPACE_B6BA536DC3EE17F4_ExpandingVariant_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::CustomTypeA& value) const {
    using into_t = typename INTO::CustomTypeA;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::CustomTypeA, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::CustomTypeB& value) const {
    using into_t = typename INTO::CustomTypeB;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::CustomTypeB, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename FROM, typename EVOLUTOR, typename VARIANT_NAME_HELPER>
struct Evolve<FROM, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B6BA536DC3EE17F4::CustomTypeA, USERSPACE_B6BA536DC3EE17F4::CustomTypeB>>, EVOLUTOR> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B6BA536DC3EE17F4, CHECK>::value>>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B6BA536DC3EE17F4::CustomTypeA, USERSPACE_B6BA536DC3EE17F4::CustomTypeB>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(USERSPACE_B6BA536DC3EE17F4_ExpandingVariant_Cases<decltype(into), FROM, INTO, EVOLUTOR>(into));
  }
};
#endif

// Default evolution for `Variant<CustomTypeA, CustomTypeB, CustomTypeC>`.
#ifndef DEFAULT_EVOLUTION_AFDB5691841C178A6C1BDC1084C4814317AFDDB153414006F070D0D92F6F357E  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B6BA536DC3EE17F4::CustomTypeA, USERSPACE_B6BA536DC3EE17F4::CustomTypeB, USERSPACE_B6BA536DC3EE17F4::CustomTypeC>>
#define DEFAULT_EVOLUTION_AFDB5691841C178A6C1BDC1084C4814317AFDDB153414006F070D0D92F6F357E  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B6BA536DC3EE17F4::CustomTypeA, USERSPACE_B6BA536DC3EE17F4::CustomTypeB, USERSPACE_B6BA536DC3EE17F4::CustomTypeC>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_B6BA536DC3EE17F4_ShrinkingVariant_Cases {
  DST& into;
  explicit USERSPACE_B6BA536DC3EE17F4_ShrinkingVariant_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::CustomTypeA& value) const {
    using into_t = typename INTO::CustomTypeA;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::CustomTypeA, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::CustomTypeB& value) const {
    using into_t = typename INTO::CustomTypeB;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::CustomTypeB, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::CustomTypeC& value) const {
    using into_t = typename INTO::CustomTypeC;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::CustomTypeC, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename FROM, typename EVOLUTOR, typename VARIANT_NAME_HELPER>
struct Evolve<FROM, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B6BA536DC3EE17F4::CustomTypeA, USERSPACE_B6BA536DC3EE17F4::CustomTypeB, USERSPACE_B6BA536DC3EE17F4::CustomTypeC>>, EVOLUTOR> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B6BA536DC3EE17F4, CHECK>::value>>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B6BA536DC3EE17F4::CustomTypeA, USERSPACE_B6BA536DC3EE17F4::CustomTypeB, USERSPACE_B6BA536DC3EE17F4::CustomTypeC>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(USERSPACE_B6BA536DC3EE17F4_ShrinkingVariant_Cases<decltype(into), FROM, INTO, EVOLUTOR>(into));
  }
};
#endif

// Default evolution for `Variant<Basic, FullName, WithOptional, WithExpandingVariant, WithShrinkingVariant, WithFieldsToRemove>`.
#ifndef DEFAULT_EVOLUTION_80B3D9E6ADEA71C8824A94734EB98E4F51763152494E8BE00B29846F955B71B2  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B6BA536DC3EE17F4::Basic, USERSPACE_B6BA536DC3EE17F4::FullName, USERSPACE_B6BA536DC3EE17F4::WithOptional, USERSPACE_B6BA536DC3EE17F4::WithExpandingVariant, USERSPACE_B6BA536DC3EE17F4::WithShrinkingVariant, USERSPACE_B6BA536DC3EE17F4::WithFieldsToRemove>>
#define DEFAULT_EVOLUTION_80B3D9E6ADEA71C8824A94734EB98E4F51763152494E8BE00B29846F955B71B2  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B6BA536DC3EE17F4::Basic, USERSPACE_B6BA536DC3EE17F4::FullName, USERSPACE_B6BA536DC3EE17F4::WithOptional, USERSPACE_B6BA536DC3EE17F4::WithExpandingVariant, USERSPACE_B6BA536DC3EE17F4::WithShrinkingVariant, USERSPACE_B6BA536DC3EE17F4::WithFieldsToRemove>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_B6BA536DC3EE17F4_All_Cases {
  DST& into;
  explicit USERSPACE_B6BA536DC3EE17F4_All_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::Basic& value) const {
    using into_t = typename INTO::Basic;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::Basic, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::FullName& value) const {
    using into_t = typename INTO::FullName;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::FullName, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::WithOptional& value) const {
    using into_t = typename INTO::WithOptional;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::WithOptional, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::WithExpandingVariant& value) const {
    using into_t = typename INTO::WithExpandingVariant;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::WithExpandingVariant, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::WithShrinkingVariant& value) const {
    using into_t = typename INTO::WithShrinkingVariant;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::WithShrinkingVariant, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::WithFieldsToRemove& value) const {
    using into_t = typename INTO::WithFieldsToRemove;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::WithFieldsToRemove, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename FROM, typename EVOLUTOR, typename VARIANT_NAME_HELPER>
struct Evolve<FROM, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B6BA536DC3EE17F4::Basic, USERSPACE_B6BA536DC3EE17F4::FullName, USERSPACE_B6BA536DC3EE17F4::WithOptional, USERSPACE_B6BA536DC3EE17F4::WithExpandingVariant, USERSPACE_B6BA536DC3EE17F4::WithShrinkingVariant, USERSPACE_B6BA536DC3EE17F4::WithFieldsToRemove>>, EVOLUTOR> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B6BA536DC3EE17F4, CHECK>::value>>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B6BA536DC3EE17F4::Basic, USERSPACE_B6BA536DC3EE17F4::FullName, USERSPACE_B6BA536DC3EE17F4::WithOptional, USERSPACE_B6BA536DC3EE17F4::WithExpandingVariant, USERSPACE_B6BA536DC3EE17F4::WithShrinkingVariant, USERSPACE_B6BA536DC3EE17F4::WithFieldsToRemove>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(USERSPACE_B6BA536DC3EE17F4_All_Cases<decltype(into), FROM, INTO, EVOLUTOR>(into));
  }
};
#endif

}  // namespace current::type_evolution
}  // namespace current

#if 0  // Boilerplate evolutors.

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, From, WithExpandingVariant, {
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.v, into.v);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, From, FullName, {
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.first_name, into.first_name);
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.last_name, into.last_name);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, From, CustomTypeB, {
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.b, into.b);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, From, Basic, {
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.i, into.i);
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.s, into.s);
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.t, into.t);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, From, TopLevel, {
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.data, into.data);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, From, WithShrinkingVariant, {
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.v, into.v);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, From, WithFieldsToRemove, {
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.foo, into.foo);
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.bar, into.bar);
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.baz, into.baz);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, From, CustomTypeA, {
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.a, into.a);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, From, CustomTypeC, {
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.c, into.c);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, From, WithOptional, {
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.maybe_name, into.maybe_name);
});

CURRENT_TYPE_EVOLUTOR_VARIANT(CustomEvolutor, From, ExpandingVariant, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(CustomTypeA, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(CustomTypeB, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
};

CURRENT_TYPE_EVOLUTOR_VARIANT(CustomEvolutor, From, ShrinkingVariant, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(CustomTypeA, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(CustomTypeB, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(CustomTypeC, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
};

CURRENT_TYPE_EVOLUTOR_VARIANT(CustomEvolutor, From, All, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(Basic, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(FullName, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(WithOptional, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(WithExpandingVariant, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(WithShrinkingVariant, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(WithFieldsToRemove, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
};

#endif  // Boilerplate evolutors.

// Privileged types.
CURRENT_DERIVED_NAMESPACE(From, USERSPACE_B6BA536DC3EE17F4) {
  CURRENT_NAMESPACE_TYPE(TopLevel, current_userspace_b6ba536dc3ee17f4::TopLevel);
};  // CURRENT_NAMESPACE(From)

// clang-format on

#endif  // CURRENT_USERSPACE_B6BA536DC3EE17F4
