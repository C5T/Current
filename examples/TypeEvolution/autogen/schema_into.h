// The `current.h` file is the one from `https://github.com/C5T/Current`.
// Compile with `-std=c++11` or higher.

#ifndef CURRENT_USERSPACE_B9C79A5A5164AA67
#define CURRENT_USERSPACE_B9C79A5A5164AA67

#include "current.h"

// clang-format off

namespace current_userspace_b9c79a5a5164aa67 {

CURRENT_STRUCT(Basic) {
  CURRENT_FIELD(i, int32_t);
  CURRENT_FIELD(s, std::string);
  CURRENT_FIELD(t, std::chrono::microseconds);
};
using T9203341832538601265 = Basic;

CURRENT_STRUCT(FullName) {
  CURRENT_FIELD(full_name, std::string);
};
using T9202391653942970634 = FullName;

CURRENT_STRUCT(WithOptional) {
  CURRENT_FIELD(maybe_name, Optional<FullName>);
};
using T9207175600672737443 = WithOptional;

CURRENT_STRUCT(CustomTypeA) {
  CURRENT_FIELD(a, int32_t);
};
using T9206911750362052937 = CustomTypeA;

CURRENT_STRUCT(CustomTypeB) {
  CURRENT_FIELD(b, int32_t);
};
using T9202573820625447155 = CustomTypeB;

CURRENT_STRUCT(CustomTypeC) {
  CURRENT_FIELD(c, int32_t);
};
using T9207934621170686053 = CustomTypeC;

CURRENT_VARIANT(ExpandingVariant, CustomTypeA, CustomTypeB, CustomTypeC);
using T9226317598374623672 = ExpandingVariant;

CURRENT_STRUCT(WithExpandingVariant) {
  CURRENT_FIELD(v, ExpandingVariant);
};
using T9205232737393522834 = WithExpandingVariant;

CURRENT_VARIANT(ShrinkingVariant, CustomTypeA, CustomTypeB);
using T9221067730773882392 = ShrinkingVariant;

CURRENT_STRUCT(WithShrinkingVariant) {
  CURRENT_FIELD(v, ShrinkingVariant);
};
using T9200848076931525722 = WithShrinkingVariant;

CURRENT_STRUCT(WithFieldsToRemove) {
  CURRENT_FIELD(foo, std::string);
  CURRENT_FIELD(bar, std::string);
};
using T9207419971064567476 = WithFieldsToRemove;

CURRENT_VARIANT(All, Basic, FullName, WithOptional, WithExpandingVariant, WithShrinkingVariant, WithFieldsToRemove);
using T9222603216121463524 = All;

CURRENT_STRUCT(TopLevel) {
  CURRENT_FIELD(data, All);
};
using T9207746704185948264 = TopLevel;

}  // namespace current_userspace_b9c79a5a5164aa67

CURRENT_NAMESPACE(USERSPACE_B9C79A5A5164AA67) {
  CURRENT_NAMESPACE_TYPE(WithShrinkingVariant, current_userspace_b9c79a5a5164aa67::WithShrinkingVariant);
  CURRENT_NAMESPACE_TYPE(FullName, current_userspace_b9c79a5a5164aa67::FullName);
  CURRENT_NAMESPACE_TYPE(CustomTypeB, current_userspace_b9c79a5a5164aa67::CustomTypeB);
  CURRENT_NAMESPACE_TYPE(Basic, current_userspace_b9c79a5a5164aa67::Basic);
  CURRENT_NAMESPACE_TYPE(WithExpandingVariant, current_userspace_b9c79a5a5164aa67::WithExpandingVariant);
  CURRENT_NAMESPACE_TYPE(CustomTypeA, current_userspace_b9c79a5a5164aa67::CustomTypeA);
  CURRENT_NAMESPACE_TYPE(WithOptional, current_userspace_b9c79a5a5164aa67::WithOptional);
  CURRENT_NAMESPACE_TYPE(WithFieldsToRemove, current_userspace_b9c79a5a5164aa67::WithFieldsToRemove);
  CURRENT_NAMESPACE_TYPE(TopLevel, current_userspace_b9c79a5a5164aa67::TopLevel);
  CURRENT_NAMESPACE_TYPE(CustomTypeC, current_userspace_b9c79a5a5164aa67::CustomTypeC);
  CURRENT_NAMESPACE_TYPE(ShrinkingVariant, current_userspace_b9c79a5a5164aa67::ShrinkingVariant);
  CURRENT_NAMESPACE_TYPE(All, current_userspace_b9c79a5a5164aa67::All);
  CURRENT_NAMESPACE_TYPE(ExpandingVariant, current_userspace_b9c79a5a5164aa67::ExpandingVariant);
};  // CURRENT_NAMESPACE(USERSPACE_B9C79A5A5164AA67)

namespace current {
namespace type_evolution {

// Default evolution for struct `WithShrinkingVariant`.
#ifndef DEFAULT_EVOLUTION_FDA43C4EC5E83FDE5D6CEA0D22726438B1EEF95BA8C173AEDA0EF9436092AA9E  // typename USERSPACE_B9C79A5A5164AA67::WithShrinkingVariant
#define DEFAULT_EVOLUTION_FDA43C4EC5E83FDE5D6CEA0D22726438B1EEF95BA8C173AEDA0EF9436092AA9E  // typename USERSPACE_B9C79A5A5164AA67::WithShrinkingVariant
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B9C79A5A5164AA67::WithShrinkingVariant, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B9C79A5A5164AA67, CHECK>::value>>
  static void Go(const typename USERSPACE_B9C79A5A5164AA67::WithShrinkingVariant& from,
                 typename INTO::WithShrinkingVariant& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::WithShrinkingVariant>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.v), EVOLUTOR>::template Go<INTO>(from.v, into.v);
  }
};
#endif

// Default evolution for struct `FullName`.
#ifndef DEFAULT_EVOLUTION_B479145B0678D9C6EC293F90D4B733B82644E3D4C9638F2D1ECF4097EB768326  // typename USERSPACE_B9C79A5A5164AA67::FullName
#define DEFAULT_EVOLUTION_B479145B0678D9C6EC293F90D4B733B82644E3D4C9638F2D1ECF4097EB768326  // typename USERSPACE_B9C79A5A5164AA67::FullName
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B9C79A5A5164AA67::FullName, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B9C79A5A5164AA67, CHECK>::value>>
  static void Go(const typename USERSPACE_B9C79A5A5164AA67::FullName& from,
                 typename INTO::FullName& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::FullName>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.full_name), EVOLUTOR>::template Go<INTO>(from.full_name, into.full_name);
  }
};
#endif

// Default evolution for struct `CustomTypeB`.
#ifndef DEFAULT_EVOLUTION_BC841BC2AAA14D8AC8B3DEA09707FD326A5AB4C877005B871480A8CDFE53B38D  // typename USERSPACE_B9C79A5A5164AA67::CustomTypeB
#define DEFAULT_EVOLUTION_BC841BC2AAA14D8AC8B3DEA09707FD326A5AB4C877005B871480A8CDFE53B38D  // typename USERSPACE_B9C79A5A5164AA67::CustomTypeB
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B9C79A5A5164AA67::CustomTypeB, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B9C79A5A5164AA67, CHECK>::value>>
  static void Go(const typename USERSPACE_B9C79A5A5164AA67::CustomTypeB& from,
                 typename INTO::CustomTypeB& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::CustomTypeB>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.b), EVOLUTOR>::template Go<INTO>(from.b, into.b);
  }
};
#endif

// Default evolution for struct `Basic`.
#ifndef DEFAULT_EVOLUTION_EAA73F0ECEEA2EA9EF98DAFE56B414124F142668812E728BBE3431AEE25D679E  // typename USERSPACE_B9C79A5A5164AA67::Basic
#define DEFAULT_EVOLUTION_EAA73F0ECEEA2EA9EF98DAFE56B414124F142668812E728BBE3431AEE25D679E  // typename USERSPACE_B9C79A5A5164AA67::Basic
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B9C79A5A5164AA67::Basic, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B9C79A5A5164AA67, CHECK>::value>>
  static void Go(const typename USERSPACE_B9C79A5A5164AA67::Basic& from,
                 typename INTO::Basic& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Basic>::value == 3,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.i), EVOLUTOR>::template Go<INTO>(from.i, into.i);
      Evolve<FROM, decltype(from.s), EVOLUTOR>::template Go<INTO>(from.s, into.s);
      Evolve<FROM, decltype(from.t), EVOLUTOR>::template Go<INTO>(from.t, into.t);
  }
};
#endif

// Default evolution for struct `WithExpandingVariant`.
#ifndef DEFAULT_EVOLUTION_10EC0B5D92FDBCA0D7A15587DD6AE4DCC4FF04DDCC583282A78922B1C441BF86  // typename USERSPACE_B9C79A5A5164AA67::WithExpandingVariant
#define DEFAULT_EVOLUTION_10EC0B5D92FDBCA0D7A15587DD6AE4DCC4FF04DDCC583282A78922B1C441BF86  // typename USERSPACE_B9C79A5A5164AA67::WithExpandingVariant
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B9C79A5A5164AA67::WithExpandingVariant, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B9C79A5A5164AA67, CHECK>::value>>
  static void Go(const typename USERSPACE_B9C79A5A5164AA67::WithExpandingVariant& from,
                 typename INTO::WithExpandingVariant& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::WithExpandingVariant>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.v), EVOLUTOR>::template Go<INTO>(from.v, into.v);
  }
};
#endif

// Default evolution for struct `CustomTypeA`.
#ifndef DEFAULT_EVOLUTION_A887C410EEFDB36359EB312C48522601CBCED456FC36EBED2BC51BD704AC6AEB  // typename USERSPACE_B9C79A5A5164AA67::CustomTypeA
#define DEFAULT_EVOLUTION_A887C410EEFDB36359EB312C48522601CBCED456FC36EBED2BC51BD704AC6AEB  // typename USERSPACE_B9C79A5A5164AA67::CustomTypeA
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B9C79A5A5164AA67::CustomTypeA, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B9C79A5A5164AA67, CHECK>::value>>
  static void Go(const typename USERSPACE_B9C79A5A5164AA67::CustomTypeA& from,
                 typename INTO::CustomTypeA& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::CustomTypeA>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.a), EVOLUTOR>::template Go<INTO>(from.a, into.a);
  }
};
#endif

// Default evolution for struct `WithOptional`.
#ifndef DEFAULT_EVOLUTION_0399AEC0CC6449C9B1542CE7C2BBFFAB1304137FD786023A079BD43AF6206F0F  // typename USERSPACE_B9C79A5A5164AA67::WithOptional
#define DEFAULT_EVOLUTION_0399AEC0CC6449C9B1542CE7C2BBFFAB1304137FD786023A079BD43AF6206F0F  // typename USERSPACE_B9C79A5A5164AA67::WithOptional
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B9C79A5A5164AA67::WithOptional, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B9C79A5A5164AA67, CHECK>::value>>
  static void Go(const typename USERSPACE_B9C79A5A5164AA67::WithOptional& from,
                 typename INTO::WithOptional& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::WithOptional>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.maybe_name), EVOLUTOR>::template Go<INTO>(from.maybe_name, into.maybe_name);
  }
};
#endif

// Default evolution for struct `WithFieldsToRemove`.
#ifndef DEFAULT_EVOLUTION_277BE7A73F0A328343DABF7621F547931999AC904D86252B3C4AE418D32C2315  // typename USERSPACE_B9C79A5A5164AA67::WithFieldsToRemove
#define DEFAULT_EVOLUTION_277BE7A73F0A328343DABF7621F547931999AC904D86252B3C4AE418D32C2315  // typename USERSPACE_B9C79A5A5164AA67::WithFieldsToRemove
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B9C79A5A5164AA67::WithFieldsToRemove, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B9C79A5A5164AA67, CHECK>::value>>
  static void Go(const typename USERSPACE_B9C79A5A5164AA67::WithFieldsToRemove& from,
                 typename INTO::WithFieldsToRemove& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::WithFieldsToRemove>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.foo), EVOLUTOR>::template Go<INTO>(from.foo, into.foo);
      Evolve<FROM, decltype(from.bar), EVOLUTOR>::template Go<INTO>(from.bar, into.bar);
  }
};
#endif

// Default evolution for struct `TopLevel`.
#ifndef DEFAULT_EVOLUTION_AEE84A17AE15F69EC9815017DE4AB6B1C56ECA98F0172B3D7C9F4AC02D1C17A7  // typename USERSPACE_B9C79A5A5164AA67::TopLevel
#define DEFAULT_EVOLUTION_AEE84A17AE15F69EC9815017DE4AB6B1C56ECA98F0172B3D7C9F4AC02D1C17A7  // typename USERSPACE_B9C79A5A5164AA67::TopLevel
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B9C79A5A5164AA67::TopLevel, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B9C79A5A5164AA67, CHECK>::value>>
  static void Go(const typename USERSPACE_B9C79A5A5164AA67::TopLevel& from,
                 typename INTO::TopLevel& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TopLevel>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.data), EVOLUTOR>::template Go<INTO>(from.data, into.data);
  }
};
#endif

// Default evolution for struct `CustomTypeC`.
#ifndef DEFAULT_EVOLUTION_8501898B56679B1D7F7763A901778B25DC7297045487F882E6AE279A1C7DBA77  // typename USERSPACE_B9C79A5A5164AA67::CustomTypeC
#define DEFAULT_EVOLUTION_8501898B56679B1D7F7763A901778B25DC7297045487F882E6AE279A1C7DBA77  // typename USERSPACE_B9C79A5A5164AA67::CustomTypeC
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B9C79A5A5164AA67::CustomTypeC, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B9C79A5A5164AA67, CHECK>::value>>
  static void Go(const typename USERSPACE_B9C79A5A5164AA67::CustomTypeC& from,
                 typename INTO::CustomTypeC& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::CustomTypeC>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.c), EVOLUTOR>::template Go<INTO>(from.c, into.c);
  }
};
#endif

// Default evolution for `Optional<FullName>`.
#ifndef DEFAULT_EVOLUTION_626C1D75DDB2C485A9134A0F9BF5F2AD7831B9C9057CB025A95CCA786934232F  // Optional<typename USERSPACE_B9C79A5A5164AA67::FullName>
#define DEFAULT_EVOLUTION_626C1D75DDB2C485A9134A0F9BF5F2AD7831B9C9057CB025A95CCA786934232F  // Optional<typename USERSPACE_B9C79A5A5164AA67::FullName>
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, Optional<typename USERSPACE_B9C79A5A5164AA67::FullName>, EVOLUTOR> {
  template <typename INTO, typename INTO_TYPE>
  static void Go(const Optional<typename USERSPACE_B9C79A5A5164AA67::FullName>& from, INTO_TYPE& into) {
    if (Exists(from)) {
      typename INTO::FullName evolved;
      Evolve<FROM, typename USERSPACE_B9C79A5A5164AA67::FullName, EVOLUTOR>::template Go<INTO>(Value(from), evolved);
      into = evolved;
    } else {
      into = nullptr;
    }
  }
};
#endif

// Default evolution for `Variant<CustomTypeA, CustomTypeB>`.
#ifndef DEFAULT_EVOLUTION_3D7645E3A0FC45AE662F8C9B827BBA7D7C381ACDE919D0E2EC68FF15F8B627F8  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B9C79A5A5164AA67::CustomTypeA, USERSPACE_B9C79A5A5164AA67::CustomTypeB>>
#define DEFAULT_EVOLUTION_3D7645E3A0FC45AE662F8C9B827BBA7D7C381ACDE919D0E2EC68FF15F8B627F8  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B9C79A5A5164AA67::CustomTypeA, USERSPACE_B9C79A5A5164AA67::CustomTypeB>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_B9C79A5A5164AA67_ShrinkingVariant_Cases {
  DST& into;
  explicit USERSPACE_B9C79A5A5164AA67_ShrinkingVariant_Cases(DST& into) : into(into) {}
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
struct Evolve<FROM, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B9C79A5A5164AA67::CustomTypeA, USERSPACE_B9C79A5A5164AA67::CustomTypeB>>, EVOLUTOR> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B9C79A5A5164AA67, CHECK>::value>>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B9C79A5A5164AA67::CustomTypeA, USERSPACE_B9C79A5A5164AA67::CustomTypeB>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(USERSPACE_B9C79A5A5164AA67_ShrinkingVariant_Cases<decltype(into), FROM, INTO, EVOLUTOR>(into));
  }
};
#endif

// Default evolution for `Variant<Basic, FullName, WithOptional, WithExpandingVariant, WithShrinkingVariant, WithFieldsToRemove>`.
#ifndef DEFAULT_EVOLUTION_8FEDD9A3064349524E1CF122D1BFA70D17967A5441B4C844E331F4C328A53144  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B9C79A5A5164AA67::Basic, USERSPACE_B9C79A5A5164AA67::FullName, USERSPACE_B9C79A5A5164AA67::WithOptional, USERSPACE_B9C79A5A5164AA67::WithExpandingVariant, USERSPACE_B9C79A5A5164AA67::WithShrinkingVariant, USERSPACE_B9C79A5A5164AA67::WithFieldsToRemove>>
#define DEFAULT_EVOLUTION_8FEDD9A3064349524E1CF122D1BFA70D17967A5441B4C844E331F4C328A53144  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B9C79A5A5164AA67::Basic, USERSPACE_B9C79A5A5164AA67::FullName, USERSPACE_B9C79A5A5164AA67::WithOptional, USERSPACE_B9C79A5A5164AA67::WithExpandingVariant, USERSPACE_B9C79A5A5164AA67::WithShrinkingVariant, USERSPACE_B9C79A5A5164AA67::WithFieldsToRemove>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_B9C79A5A5164AA67_All_Cases {
  DST& into;
  explicit USERSPACE_B9C79A5A5164AA67_All_Cases(DST& into) : into(into) {}
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
struct Evolve<FROM, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B9C79A5A5164AA67::Basic, USERSPACE_B9C79A5A5164AA67::FullName, USERSPACE_B9C79A5A5164AA67::WithOptional, USERSPACE_B9C79A5A5164AA67::WithExpandingVariant, USERSPACE_B9C79A5A5164AA67::WithShrinkingVariant, USERSPACE_B9C79A5A5164AA67::WithFieldsToRemove>>, EVOLUTOR> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B9C79A5A5164AA67, CHECK>::value>>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B9C79A5A5164AA67::Basic, USERSPACE_B9C79A5A5164AA67::FullName, USERSPACE_B9C79A5A5164AA67::WithOptional, USERSPACE_B9C79A5A5164AA67::WithExpandingVariant, USERSPACE_B9C79A5A5164AA67::WithShrinkingVariant, USERSPACE_B9C79A5A5164AA67::WithFieldsToRemove>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(USERSPACE_B9C79A5A5164AA67_All_Cases<decltype(into), FROM, INTO, EVOLUTOR>(into));
  }
};
#endif

// Default evolution for `Variant<CustomTypeA, CustomTypeB, CustomTypeC>`.
#ifndef DEFAULT_EVOLUTION_5D0D63984925D2A3A6F89387245465D727EF55AB64E8B0C43E262D481ACA79B6  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B9C79A5A5164AA67::CustomTypeA, USERSPACE_B9C79A5A5164AA67::CustomTypeB, USERSPACE_B9C79A5A5164AA67::CustomTypeC>>
#define DEFAULT_EVOLUTION_5D0D63984925D2A3A6F89387245465D727EF55AB64E8B0C43E262D481ACA79B6  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B9C79A5A5164AA67::CustomTypeA, USERSPACE_B9C79A5A5164AA67::CustomTypeB, USERSPACE_B9C79A5A5164AA67::CustomTypeC>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_B9C79A5A5164AA67_ExpandingVariant_Cases {
  DST& into;
  explicit USERSPACE_B9C79A5A5164AA67_ExpandingVariant_Cases(DST& into) : into(into) {}
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
struct Evolve<FROM, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B9C79A5A5164AA67::CustomTypeA, USERSPACE_B9C79A5A5164AA67::CustomTypeB, USERSPACE_B9C79A5A5164AA67::CustomTypeC>>, EVOLUTOR> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B9C79A5A5164AA67, CHECK>::value>>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B9C79A5A5164AA67::CustomTypeA, USERSPACE_B9C79A5A5164AA67::CustomTypeB, USERSPACE_B9C79A5A5164AA67::CustomTypeC>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(USERSPACE_B9C79A5A5164AA67_ExpandingVariant_Cases<decltype(into), FROM, INTO, EVOLUTOR>(into));
  }
};
#endif

}  // namespace current::type_evolution
}  // namespace current

#if 0  // Boilerplate evolutors.

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, Into, WithShrinkingVariant, {
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.v, into.v);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, Into, FullName, {
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.full_name, into.full_name);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, Into, CustomTypeB, {
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.b, into.b);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, Into, Basic, {
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.i, into.i);
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.s, into.s);
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.t, into.t);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, Into, WithExpandingVariant, {
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.v, into.v);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, Into, CustomTypeA, {
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.a, into.a);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, Into, WithOptional, {
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.maybe_name, into.maybe_name);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, Into, WithFieldsToRemove, {
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.foo, into.foo);
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.bar, into.bar);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, Into, TopLevel, {
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.data, into.data);
});

CURRENT_TYPE_EVOLUTOR(CustomEvolutor, Into, CustomTypeC, {
  CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from.c, into.c);
});

CURRENT_TYPE_EVOLUTOR_VARIANT(CustomEvolutor, Into, ShrinkingVariant, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(CustomTypeA, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(CustomTypeB, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
};

CURRENT_TYPE_EVOLUTOR_VARIANT(CustomEvolutor, Into, All, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(Basic, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(FullName, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(WithOptional, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(WithExpandingVariant, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(WithShrinkingVariant, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(WithFieldsToRemove, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
};

CURRENT_TYPE_EVOLUTOR_VARIANT(CustomEvolutor, Into, ExpandingVariant, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(CustomTypeA, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(CustomTypeB, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(CustomTypeC, CURRENT_NATURAL_EVOLVE(Into, CustomDestinationNamespace, from, into));
};

#endif  // Boilerplate evolutors.

// Privileged types.
CURRENT_DERIVED_NAMESPACE(Into, USERSPACE_B9C79A5A5164AA67) {
  CURRENT_NAMESPACE_TYPE(TopLevel, current_userspace_b9c79a5a5164aa67::TopLevel);
};  // CURRENT_NAMESPACE(Into)

// clang-format on

#endif  // CURRENT_USERSPACE_B9C79A5A5164AA67
