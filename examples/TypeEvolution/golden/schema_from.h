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

#ifndef CURRENT_SCHEMA_FOR_T9201952458119363219
#define CURRENT_SCHEMA_FOR_T9201952458119363219
namespace t9201952458119363219 {
CURRENT_STRUCT(FullName) {
  CURRENT_FIELD(first_name, std::string);
  CURRENT_FIELD(last_name, std::string);
};
}  // namespace t9201952458119363219
#endif  // CURRENT_SCHEMA_FOR_T_9201952458119363219

#ifndef CURRENT_SCHEMA_FOR_T9209615657629583566
#define CURRENT_SCHEMA_FOR_T9209615657629583566
namespace t9209615657629583566 {
CURRENT_STRUCT(WithOptional) {
  CURRENT_FIELD(maybe_name, Optional<t9201952458119363219::FullName>);
};
}  // namespace t9209615657629583566
#endif  // CURRENT_SCHEMA_FOR_T_9209615657629583566

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

#ifndef CURRENT_SCHEMA_FOR_T9221067729821834539
#define CURRENT_SCHEMA_FOR_T9221067729821834539
namespace t9221067729821834539 {
CURRENT_VARIANT(ExpandingVariant, t9206911750362052937::CustomTypeA, t9202573820625447155::CustomTypeB);
}  // namespace t9221067729821834539
#endif  // CURRENT_SCHEMA_FOR_T_9221067729821834539

#ifndef CURRENT_SCHEMA_FOR_T9201009161779005289
#define CURRENT_SCHEMA_FOR_T9201009161779005289
namespace t9201009161779005289 {
CURRENT_STRUCT(WithExpandingVariant) {
  CURRENT_FIELD(v, t9221067729821834539::ExpandingVariant);
};
}  // namespace t9201009161779005289
#endif  // CURRENT_SCHEMA_FOR_T_9201009161779005289

#ifndef CURRENT_SCHEMA_FOR_T9207934621170686053
#define CURRENT_SCHEMA_FOR_T9207934621170686053
namespace t9207934621170686053 {
CURRENT_STRUCT(CustomTypeC) {
  CURRENT_FIELD(c, int32_t);
};
}  // namespace t9207934621170686053
#endif  // CURRENT_SCHEMA_FOR_T_9207934621170686053

#ifndef CURRENT_SCHEMA_FOR_T9226317599863657099
#define CURRENT_SCHEMA_FOR_T9226317599863657099
namespace t9226317599863657099 {
CURRENT_VARIANT(ShrinkingVariant, t9206911750362052937::CustomTypeA, t9202573820625447155::CustomTypeB, t9207934621170686053::CustomTypeC);
}  // namespace t9226317599863657099
#endif  // CURRENT_SCHEMA_FOR_T_9226317599863657099

#ifndef CURRENT_SCHEMA_FOR_T9205425707876881313
#define CURRENT_SCHEMA_FOR_T9205425707876881313
namespace t9205425707876881313 {
CURRENT_STRUCT(WithShrinkingVariant) {
  CURRENT_FIELD(v, t9226317599863657099::ShrinkingVariant);
};
}  // namespace t9205425707876881313
#endif  // CURRENT_SCHEMA_FOR_T_9205425707876881313

#ifndef CURRENT_SCHEMA_FOR_T9206820554754825223
#define CURRENT_SCHEMA_FOR_T9206820554754825223
namespace t9206820554754825223 {
CURRENT_STRUCT(WithFieldsToRemove) {
  CURRENT_FIELD(foo, std::string);
  CURRENT_FIELD(bar, std::string);
  CURRENT_FIELD(baz, std::vector<std::string>);
};
}  // namespace t9206820554754825223
#endif  // CURRENT_SCHEMA_FOR_T_9206820554754825223

#ifndef CURRENT_SCHEMA_FOR_T9227074222822990824
#define CURRENT_SCHEMA_FOR_T9227074222822990824
namespace t9227074222822990824 {
CURRENT_VARIANT(All, t9203341832538601265::Basic, t9201952458119363219::FullName, t9209615657629583566::WithOptional, t9201009161779005289::WithExpandingVariant, t9205425707876881313::WithShrinkingVariant, t9206820554754825223::WithFieldsToRemove);
}  // namespace t9227074222822990824
#endif  // CURRENT_SCHEMA_FOR_T_9227074222822990824

#ifndef CURRENT_SCHEMA_FOR_T9204063025278945160
#define CURRENT_SCHEMA_FOR_T9204063025278945160
namespace t9204063025278945160 {
CURRENT_STRUCT(TopLevel) {
  CURRENT_FIELD(data, t9227074222822990824::All);
};
}  // namespace t9204063025278945160
#endif  // CURRENT_SCHEMA_FOR_T_9204063025278945160

}  // namespace current_userspace

#ifndef CURRENT_NAMESPACE_From_DEFINED
CURRENT_NAMESPACE(From) {
  CURRENT_NAMESPACE_TYPE(WithExpandingVariant, current_userspace::t9201009161779005289::WithExpandingVariant);
  CURRENT_NAMESPACE_TYPE(FullName, current_userspace::t9201952458119363219::FullName);
  CURRENT_NAMESPACE_TYPE(CustomTypeB, current_userspace::t9202573820625447155::CustomTypeB);
  CURRENT_NAMESPACE_TYPE(Basic, current_userspace::t9203341832538601265::Basic);
  CURRENT_NAMESPACE_TYPE(TopLevel, current_userspace::t9204063025278945160::TopLevel);
  CURRENT_NAMESPACE_TYPE(WithShrinkingVariant, current_userspace::t9205425707876881313::WithShrinkingVariant);
  CURRENT_NAMESPACE_TYPE(WithFieldsToRemove, current_userspace::t9206820554754825223::WithFieldsToRemove);
  CURRENT_NAMESPACE_TYPE(CustomTypeA, current_userspace::t9206911750362052937::CustomTypeA);
  CURRENT_NAMESPACE_TYPE(CustomTypeC, current_userspace::t9207934621170686053::CustomTypeC);
  CURRENT_NAMESPACE_TYPE(WithOptional, current_userspace::t9209615657629583566::WithOptional);
  CURRENT_NAMESPACE_TYPE(ExpandingVariant, current_userspace::t9221067729821834539::ExpandingVariant);
  CURRENT_NAMESPACE_TYPE(ShrinkingVariant, current_userspace::t9226317599863657099::ShrinkingVariant);
  CURRENT_NAMESPACE_TYPE(All, current_userspace::t9227074222822990824::All);

  // Privileged types.
  CURRENT_NAMESPACE_TYPE(ExposedTopLevel, current_userspace::t9204063025278945160::TopLevel);
};  // CURRENT_NAMESPACE(From)
#endif  // CURRENT_NAMESPACE_From_DEFINED

namespace current {
namespace type_evolution {

// Default evolution for struct `WithExpandingVariant`.
#ifndef DEFAULT_EVOLUTION_B496AA86275A67D9F4C7EF1DB01D57EDD22C16E4A98320F8BD5B1815942A10ED  // typename From::WithExpandingVariant
#define DEFAULT_EVOLUTION_B496AA86275A67D9F4C7EF1DB01D57EDD22C16E4A98320F8BD5B1815942A10ED  // typename From::WithExpandingVariant
template <typename EVOLVER>
struct Evolve<From, typename From::WithExpandingVariant, EVOLVER> {
  template <typename INTO>
  static void Go(const typename From::WithExpandingVariant& from,
                 typename INTO::WithExpandingVariant& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::WithExpandingVariant>::value == 1,
                    "Custom evolver required.");
      Evolve<From, decltype(from.v), EVOLVER>::template Go<INTO>(from.v, into.v);
  }
};
#endif

// Default evolution for struct `FullName`.
#ifndef DEFAULT_EVOLUTION_6577A8C63F5A4B2F68562B7B359EF1805EBB39C7BDA6711589F1E64C033C9790  // typename From::FullName
#define DEFAULT_EVOLUTION_6577A8C63F5A4B2F68562B7B359EF1805EBB39C7BDA6711589F1E64C033C9790  // typename From::FullName
template <typename EVOLVER>
struct Evolve<From, typename From::FullName, EVOLVER> {
  template <typename INTO>
  static void Go(const typename From::FullName& from,
                 typename INTO::FullName& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::FullName>::value == 2,
                    "Custom evolver required.");
      Evolve<From, decltype(from.first_name), EVOLVER>::template Go<INTO>(from.first_name, into.first_name);
      Evolve<From, decltype(from.last_name), EVOLVER>::template Go<INTO>(from.last_name, into.last_name);
  }
};
#endif

// Default evolution for struct `CustomTypeB`.
#ifndef DEFAULT_EVOLUTION_B4F7587FE0CBAD1306A8E6AF1B4A23BCCC6B9D1CD27445C2D3A62DFB83ADB03F  // typename From::CustomTypeB
#define DEFAULT_EVOLUTION_B4F7587FE0CBAD1306A8E6AF1B4A23BCCC6B9D1CD27445C2D3A62DFB83ADB03F  // typename From::CustomTypeB
template <typename EVOLVER>
struct Evolve<From, typename From::CustomTypeB, EVOLVER> {
  template <typename INTO>
  static void Go(const typename From::CustomTypeB& from,
                 typename INTO::CustomTypeB& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::CustomTypeB>::value == 1,
                    "Custom evolver required.");
      Evolve<From, decltype(from.b), EVOLVER>::template Go<INTO>(from.b, into.b);
  }
};
#endif

// Default evolution for struct `Basic`.
#ifndef DEFAULT_EVOLUTION_CF5D17E6C58FB51A713EAB6FDCC0DB8E60A8DDE553FDD41139224BD19A0C4947  // typename From::Basic
#define DEFAULT_EVOLUTION_CF5D17E6C58FB51A713EAB6FDCC0DB8E60A8DDE553FDD41139224BD19A0C4947  // typename From::Basic
template <typename EVOLVER>
struct Evolve<From, typename From::Basic, EVOLVER> {
  template <typename INTO>
  static void Go(const typename From::Basic& from,
                 typename INTO::Basic& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Basic>::value == 3,
                    "Custom evolver required.");
      Evolve<From, decltype(from.i), EVOLVER>::template Go<INTO>(from.i, into.i);
      Evolve<From, decltype(from.s), EVOLVER>::template Go<INTO>(from.s, into.s);
      Evolve<From, decltype(from.t), EVOLVER>::template Go<INTO>(from.t, into.t);
  }
};
#endif

// Default evolution for struct `TopLevel`.
#ifndef DEFAULT_EVOLUTION_C287C195670345584E3E28963FBCB9135C02DC52F5925249ABAC277413CC8795  // typename From::TopLevel
#define DEFAULT_EVOLUTION_C287C195670345584E3E28963FBCB9135C02DC52F5925249ABAC277413CC8795  // typename From::TopLevel
template <typename EVOLVER>
struct Evolve<From, typename From::TopLevel, EVOLVER> {
  template <typename INTO>
  static void Go(const typename From::TopLevel& from,
                 typename INTO::TopLevel& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TopLevel>::value == 1,
                    "Custom evolver required.");
      Evolve<From, decltype(from.data), EVOLVER>::template Go<INTO>(from.data, into.data);
  }
};
#endif

// Default evolution for struct `WithShrinkingVariant`.
#ifndef DEFAULT_EVOLUTION_0A1C6EDB7AB8582E2505B02D8534CCBC3D3539389F050AFBA639474CEDAEA2D9  // typename From::WithShrinkingVariant
#define DEFAULT_EVOLUTION_0A1C6EDB7AB8582E2505B02D8534CCBC3D3539389F050AFBA639474CEDAEA2D9  // typename From::WithShrinkingVariant
template <typename EVOLVER>
struct Evolve<From, typename From::WithShrinkingVariant, EVOLVER> {
  template <typename INTO>
  static void Go(const typename From::WithShrinkingVariant& from,
                 typename INTO::WithShrinkingVariant& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::WithShrinkingVariant>::value == 1,
                    "Custom evolver required.");
      Evolve<From, decltype(from.v), EVOLVER>::template Go<INTO>(from.v, into.v);
  }
};
#endif

// Default evolution for struct `WithFieldsToRemove`.
#ifndef DEFAULT_EVOLUTION_FB411CF04FCA3A6F22CDD114FF27D733B09B31ED5059D6670A8B4C484D08ED3C  // typename From::WithFieldsToRemove
#define DEFAULT_EVOLUTION_FB411CF04FCA3A6F22CDD114FF27D733B09B31ED5059D6670A8B4C484D08ED3C  // typename From::WithFieldsToRemove
template <typename EVOLVER>
struct Evolve<From, typename From::WithFieldsToRemove, EVOLVER> {
  template <typename INTO>
  static void Go(const typename From::WithFieldsToRemove& from,
                 typename INTO::WithFieldsToRemove& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::WithFieldsToRemove>::value == 3,
                    "Custom evolver required.");
      Evolve<From, decltype(from.foo), EVOLVER>::template Go<INTO>(from.foo, into.foo);
      Evolve<From, decltype(from.bar), EVOLVER>::template Go<INTO>(from.bar, into.bar);
      Evolve<From, decltype(from.baz), EVOLVER>::template Go<INTO>(from.baz, into.baz);
  }
};
#endif

// Default evolution for struct `CustomTypeA`.
#ifndef DEFAULT_EVOLUTION_F9104A2D9C1D1AD7EBA0D769759C414CEEC91E6F67AE55969B2FD0FA225811F8  // typename From::CustomTypeA
#define DEFAULT_EVOLUTION_F9104A2D9C1D1AD7EBA0D769759C414CEEC91E6F67AE55969B2FD0FA225811F8  // typename From::CustomTypeA
template <typename EVOLVER>
struct Evolve<From, typename From::CustomTypeA, EVOLVER> {
  template <typename INTO>
  static void Go(const typename From::CustomTypeA& from,
                 typename INTO::CustomTypeA& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::CustomTypeA>::value == 1,
                    "Custom evolver required.");
      Evolve<From, decltype(from.a), EVOLVER>::template Go<INTO>(from.a, into.a);
  }
};
#endif

// Default evolution for struct `CustomTypeC`.
#ifndef DEFAULT_EVOLUTION_F857EC95C5A6D5456B78F905565F3EB0B0D530571DBBB8D2DAC34472C0F6462F  // typename From::CustomTypeC
#define DEFAULT_EVOLUTION_F857EC95C5A6D5456B78F905565F3EB0B0D530571DBBB8D2DAC34472C0F6462F  // typename From::CustomTypeC
template <typename EVOLVER>
struct Evolve<From, typename From::CustomTypeC, EVOLVER> {
  template <typename INTO>
  static void Go(const typename From::CustomTypeC& from,
                 typename INTO::CustomTypeC& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::CustomTypeC>::value == 1,
                    "Custom evolver required.");
      Evolve<From, decltype(from.c), EVOLVER>::template Go<INTO>(from.c, into.c);
  }
};
#endif

// Default evolution for struct `WithOptional`.
#ifndef DEFAULT_EVOLUTION_0EDF8BC44C2CA1505F6D61005DF01A3D206E349509AFE045970771886618F01A  // typename From::WithOptional
#define DEFAULT_EVOLUTION_0EDF8BC44C2CA1505F6D61005DF01A3D206E349509AFE045970771886618F01A  // typename From::WithOptional
template <typename EVOLVER>
struct Evolve<From, typename From::WithOptional, EVOLVER> {
  template <typename INTO>
  static void Go(const typename From::WithOptional& from,
                 typename INTO::WithOptional& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::WithOptional>::value == 1,
                    "Custom evolver required.");
      Evolve<From, decltype(from.maybe_name), EVOLVER>::template Go<INTO>(from.maybe_name, into.maybe_name);
  }
};
#endif

// Default evolution for `Optional<t9201952458119363219::FullName>`.
#ifndef DEFAULT_EVOLUTION_7F21884FD1D375CB1AAF7E44AF4D6A3EA8C86BF3B56C276FBE1AE0936192B26C  // Optional<typename From::FullName>
#define DEFAULT_EVOLUTION_7F21884FD1D375CB1AAF7E44AF4D6A3EA8C86BF3B56C276FBE1AE0936192B26C  // Optional<typename From::FullName>
template <typename EVOLVER>
struct Evolve<From, Optional<typename From::FullName>, EVOLVER> {
  template <typename INTO, typename INTO_TYPE>
  static void Go(const Optional<typename From::FullName>& from, INTO_TYPE& into) {
    if (Exists(from)) {
      typename INTO::FullName evolved;
      Evolve<From, typename From::FullName, EVOLVER>::template Go<INTO>(Value(from), evolved);
      into = evolved;
    } else {
      into = nullptr;
    }
  }
};
#endif

// Default evolution for `Variant<CustomTypeA, CustomTypeB>`.
#ifndef DEFAULT_EVOLUTION_650CD48C6CB00F02DC05D209DA289756B834242D11C7B86B7F9ECD3475DAB814  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<From::CustomTypeA, From::CustomTypeB>>
#define DEFAULT_EVOLUTION_650CD48C6CB00F02DC05D209DA289756B834242D11C7B86B7F9ECD3475DAB814  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<From::CustomTypeA, From::CustomTypeB>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLVER>
struct From_ExpandingVariant_Cases {
  DST& into;
  explicit From_ExpandingVariant_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::CustomTypeA& value) const {
    using into_t = typename INTO::CustomTypeA;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::CustomTypeA, EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::CustomTypeB& value) const {
    using into_t = typename INTO::CustomTypeB;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::CustomTypeB, EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename EVOLVER, typename VARIANT_NAME_HELPER>
struct Evolve<From, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<From::CustomTypeA, From::CustomTypeB>>, EVOLVER> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<From::CustomTypeA, From::CustomTypeB>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(From_ExpandingVariant_Cases<decltype(into), From, INTO, EVOLVER>(into));
  }
};
#endif

// Default evolution for `Variant<CustomTypeA, CustomTypeB, CustomTypeC>`.
#ifndef DEFAULT_EVOLUTION_094E6600AB7A3F33BD7D6FDCD74BEE64B6933D2103B9039D1BF77740CC854901  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<From::CustomTypeA, From::CustomTypeB, From::CustomTypeC>>
#define DEFAULT_EVOLUTION_094E6600AB7A3F33BD7D6FDCD74BEE64B6933D2103B9039D1BF77740CC854901  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<From::CustomTypeA, From::CustomTypeB, From::CustomTypeC>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLVER>
struct From_ShrinkingVariant_Cases {
  DST& into;
  explicit From_ShrinkingVariant_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::CustomTypeA& value) const {
    using into_t = typename INTO::CustomTypeA;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::CustomTypeA, EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::CustomTypeB& value) const {
    using into_t = typename INTO::CustomTypeB;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::CustomTypeB, EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::CustomTypeC& value) const {
    using into_t = typename INTO::CustomTypeC;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::CustomTypeC, EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename EVOLVER, typename VARIANT_NAME_HELPER>
struct Evolve<From, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<From::CustomTypeA, From::CustomTypeB, From::CustomTypeC>>, EVOLVER> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<From::CustomTypeA, From::CustomTypeB, From::CustomTypeC>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(From_ShrinkingVariant_Cases<decltype(into), From, INTO, EVOLVER>(into));
  }
};
#endif

// Default evolution for `Variant<Basic, FullName, WithOptional, WithExpandingVariant, WithShrinkingVariant, WithFieldsToRemove>`.
#ifndef DEFAULT_EVOLUTION_58BD551664EB46378ED6EDA9EBE68599DDBA86E20A296CF15797BD3E9C8A9195  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<From::Basic, From::FullName, From::WithOptional, From::WithExpandingVariant, From::WithShrinkingVariant, From::WithFieldsToRemove>>
#define DEFAULT_EVOLUTION_58BD551664EB46378ED6EDA9EBE68599DDBA86E20A296CF15797BD3E9C8A9195  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<From::Basic, From::FullName, From::WithOptional, From::WithExpandingVariant, From::WithShrinkingVariant, From::WithFieldsToRemove>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLVER>
struct From_All_Cases {
  DST& into;
  explicit From_All_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::Basic& value) const {
    using into_t = typename INTO::Basic;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::Basic, EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::FullName& value) const {
    using into_t = typename INTO::FullName;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::FullName, EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::WithOptional& value) const {
    using into_t = typename INTO::WithOptional;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::WithOptional, EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::WithExpandingVariant& value) const {
    using into_t = typename INTO::WithExpandingVariant;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::WithExpandingVariant, EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::WithShrinkingVariant& value) const {
    using into_t = typename INTO::WithShrinkingVariant;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::WithShrinkingVariant, EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::WithFieldsToRemove& value) const {
    using into_t = typename INTO::WithFieldsToRemove;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::WithFieldsToRemove, EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename EVOLVER, typename VARIANT_NAME_HELPER>
struct Evolve<From, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<From::Basic, From::FullName, From::WithOptional, From::WithExpandingVariant, From::WithShrinkingVariant, From::WithFieldsToRemove>>, EVOLVER> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<From::Basic, From::FullName, From::WithOptional, From::WithExpandingVariant, From::WithShrinkingVariant, From::WithFieldsToRemove>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(From_All_Cases<decltype(into), From, INTO, EVOLVER>(into));
  }
};
#endif

}  // namespace current::type_evolution
}  // namespace current

#if 0  // Boilerplate evolvers.

CURRENT_TYPE_EVOLVER(CustomEvolver, From, WithExpandingVariant, {
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.v, into.v);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, From, FullName, {
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.first_name, into.first_name);
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.last_name, into.last_name);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, From, CustomTypeB, {
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.b, into.b);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, From, Basic, {
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.i, into.i);
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.s, into.s);
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.t, into.t);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, From, TopLevel, {
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.data, into.data);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, From, WithShrinkingVariant, {
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.v, into.v);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, From, WithFieldsToRemove, {
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.foo, into.foo);
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.bar, into.bar);
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.baz, into.baz);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, From, CustomTypeA, {
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.a, into.a);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, From, CustomTypeC, {
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.c, into.c);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, From, WithOptional, {
  CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from.maybe_name, into.maybe_name);
});

CURRENT_TYPE_EVOLVER_VARIANT(CustomEvolver, From, t9221067729821834539::ExpandingVariant, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9206911750362052937::CustomTypeA, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9202573820625447155::CustomTypeB, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
};

CURRENT_TYPE_EVOLVER_VARIANT(CustomEvolver, From, t9226317599863657099::ShrinkingVariant, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9206911750362052937::CustomTypeA, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9202573820625447155::CustomTypeB, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9207934621170686053::CustomTypeC, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
};

CURRENT_TYPE_EVOLVER_VARIANT(CustomEvolver, From, t9227074222822990824::All, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9203341832538601265::Basic, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9201952458119363219::FullName, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9209615657629583566::WithOptional, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9201009161779005289::WithExpandingVariant, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9205425707876881313::WithShrinkingVariant, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9206820554754825223::WithFieldsToRemove, CURRENT_NATURAL_EVOLVE(From, CustomDestinationNamespace, from, into));
};

#endif  // Boilerplate evolvers.

// clang-format on
