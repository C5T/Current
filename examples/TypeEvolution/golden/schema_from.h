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

#ifndef CURRENT_NAMESPACE_SchemaFrom_DEFINED
#define CURRENT_NAMESPACE_SchemaFrom_DEFINED
CURRENT_NAMESPACE(SchemaFrom) {
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
};  // CURRENT_NAMESPACE(SchemaFrom)
#endif  // CURRENT_NAMESPACE_SchemaFrom_DEFINED

namespace current {
namespace type_evolution {

// Default evolution for struct `WithExpandingVariant`.
#ifndef DEFAULT_EVOLUTION_35F5A941A0ED4942FECC6E63DFA2D8C223562074D8D02274C5D143FE7E432EB7  // typename SchemaFrom::WithExpandingVariant
#define DEFAULT_EVOLUTION_35F5A941A0ED4942FECC6E63DFA2D8C223562074D8D02274C5D143FE7E432EB7  // typename SchemaFrom::WithExpandingVariant
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaFrom, typename SchemaFrom::WithExpandingVariant, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaFrom;
  template <typename INTO>
  static void Go(const typename FROM::WithExpandingVariant& from,
                 typename INTO::WithExpandingVariant& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::WithExpandingVariant>::value == 1,
                    "Custom evolver required.");
      CURRENT_COPY_FIELD(v);
  }
};
#endif

// Default evolution for struct `FullName`.
#ifndef DEFAULT_EVOLUTION_66042ACBA01DD67D1DDF56EA363D84F20A1818A3611D2DA2FF96E5A4DC929992  // typename SchemaFrom::FullName
#define DEFAULT_EVOLUTION_66042ACBA01DD67D1DDF56EA363D84F20A1818A3611D2DA2FF96E5A4DC929992  // typename SchemaFrom::FullName
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaFrom, typename SchemaFrom::FullName, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaFrom;
  template <typename INTO>
  static void Go(const typename FROM::FullName& from,
                 typename INTO::FullName& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::FullName>::value == 2,
                    "Custom evolver required.");
      CURRENT_COPY_FIELD(first_name);
      CURRENT_COPY_FIELD(last_name);
  }
};
#endif

// Default evolution for struct `CustomTypeB`.
#ifndef DEFAULT_EVOLUTION_5A820F56502A0253198EFE13FFC71EF97DCBA659C25B9520261053DB65C3F024  // typename SchemaFrom::CustomTypeB
#define DEFAULT_EVOLUTION_5A820F56502A0253198EFE13FFC71EF97DCBA659C25B9520261053DB65C3F024  // typename SchemaFrom::CustomTypeB
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaFrom, typename SchemaFrom::CustomTypeB, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaFrom;
  template <typename INTO>
  static void Go(const typename FROM::CustomTypeB& from,
                 typename INTO::CustomTypeB& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::CustomTypeB>::value == 1,
                    "Custom evolver required.");
      CURRENT_COPY_FIELD(b);
  }
};
#endif

// Default evolution for struct `Basic`.
#ifndef DEFAULT_EVOLUTION_F7D74FC6BC82D9E53885B8228503AD398D313D83361CF909DF24C4105AB12BD9  // typename SchemaFrom::Basic
#define DEFAULT_EVOLUTION_F7D74FC6BC82D9E53885B8228503AD398D313D83361CF909DF24C4105AB12BD9  // typename SchemaFrom::Basic
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaFrom, typename SchemaFrom::Basic, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaFrom;
  template <typename INTO>
  static void Go(const typename FROM::Basic& from,
                 typename INTO::Basic& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Basic>::value == 3,
                    "Custom evolver required.");
      CURRENT_COPY_FIELD(i);
      CURRENT_COPY_FIELD(s);
      CURRENT_COPY_FIELD(t);
  }
};
#endif

// Default evolution for struct `TopLevel`.
#ifndef DEFAULT_EVOLUTION_98EEA0A52D6D214DEDC8282A45096D9B498C39B8E13FFD331D75788D8894B494  // typename SchemaFrom::TopLevel
#define DEFAULT_EVOLUTION_98EEA0A52D6D214DEDC8282A45096D9B498C39B8E13FFD331D75788D8894B494  // typename SchemaFrom::TopLevel
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaFrom, typename SchemaFrom::TopLevel, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaFrom;
  template <typename INTO>
  static void Go(const typename FROM::TopLevel& from,
                 typename INTO::TopLevel& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TopLevel>::value == 1,
                    "Custom evolver required.");
      CURRENT_COPY_FIELD(data);
  }
};
#endif

// Default evolution for struct `WithShrinkingVariant`.
#ifndef DEFAULT_EVOLUTION_45BEACF7605966792E840E096C0468AB6E7D9D26E42DD366941E1ED24DBEB5B1  // typename SchemaFrom::WithShrinkingVariant
#define DEFAULT_EVOLUTION_45BEACF7605966792E840E096C0468AB6E7D9D26E42DD366941E1ED24DBEB5B1  // typename SchemaFrom::WithShrinkingVariant
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaFrom, typename SchemaFrom::WithShrinkingVariant, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaFrom;
  template <typename INTO>
  static void Go(const typename FROM::WithShrinkingVariant& from,
                 typename INTO::WithShrinkingVariant& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::WithShrinkingVariant>::value == 1,
                    "Custom evolver required.");
      CURRENT_COPY_FIELD(v);
  }
};
#endif

// Default evolution for struct `WithFieldsToRemove`.
#ifndef DEFAULT_EVOLUTION_27CD733D52DB87243D43BEC1602B40D06F0150A1CE07B4E1DB2C0FC6455300D8  // typename SchemaFrom::WithFieldsToRemove
#define DEFAULT_EVOLUTION_27CD733D52DB87243D43BEC1602B40D06F0150A1CE07B4E1DB2C0FC6455300D8  // typename SchemaFrom::WithFieldsToRemove
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaFrom, typename SchemaFrom::WithFieldsToRemove, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaFrom;
  template <typename INTO>
  static void Go(const typename FROM::WithFieldsToRemove& from,
                 typename INTO::WithFieldsToRemove& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::WithFieldsToRemove>::value == 3,
                    "Custom evolver required.");
      CURRENT_COPY_FIELD(foo);
      CURRENT_COPY_FIELD(bar);
      CURRENT_COPY_FIELD(baz);
  }
};
#endif

// Default evolution for struct `CustomTypeA`.
#ifndef DEFAULT_EVOLUTION_0DA540E2D74B5EE31A2EEEEFA415017C2C594EAF9CAE0B9C700704BFEB7F9482  // typename SchemaFrom::CustomTypeA
#define DEFAULT_EVOLUTION_0DA540E2D74B5EE31A2EEEEFA415017C2C594EAF9CAE0B9C700704BFEB7F9482  // typename SchemaFrom::CustomTypeA
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaFrom, typename SchemaFrom::CustomTypeA, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaFrom;
  template <typename INTO>
  static void Go(const typename FROM::CustomTypeA& from,
                 typename INTO::CustomTypeA& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::CustomTypeA>::value == 1,
                    "Custom evolver required.");
      CURRENT_COPY_FIELD(a);
  }
};
#endif

// Default evolution for struct `CustomTypeC`.
#ifndef DEFAULT_EVOLUTION_673D90B67C9AAEC101B4A3E5FB9A7B89E5FFE9419093226C1BDE8CCA18F8B86B  // typename SchemaFrom::CustomTypeC
#define DEFAULT_EVOLUTION_673D90B67C9AAEC101B4A3E5FB9A7B89E5FFE9419093226C1BDE8CCA18F8B86B  // typename SchemaFrom::CustomTypeC
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaFrom, typename SchemaFrom::CustomTypeC, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaFrom;
  template <typename INTO>
  static void Go(const typename FROM::CustomTypeC& from,
                 typename INTO::CustomTypeC& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::CustomTypeC>::value == 1,
                    "Custom evolver required.");
      CURRENT_COPY_FIELD(c);
  }
};
#endif

// Default evolution for struct `WithOptional`.
#ifndef DEFAULT_EVOLUTION_77615C18DE2F83FF511E15D802C7D649923CA4DEECAE459F980CBFB2108D24B3  // typename SchemaFrom::WithOptional
#define DEFAULT_EVOLUTION_77615C18DE2F83FF511E15D802C7D649923CA4DEECAE459F980CBFB2108D24B3  // typename SchemaFrom::WithOptional
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaFrom, typename SchemaFrom::WithOptional, CURRENT_ACTIVE_EVOLVER> {
  using FROM = SchemaFrom;
  template <typename INTO>
  static void Go(const typename FROM::WithOptional& from,
                 typename INTO::WithOptional& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::WithOptional>::value == 1,
                    "Custom evolver required.");
      CURRENT_COPY_FIELD(maybe_name);
  }
};
#endif

// Default evolution for `Optional<t9201952458119363219::FullName>`.
#ifndef DEFAULT_EVOLUTION_3D54C54053A3933A75E38E133177491491E5DE57D28B91809F26CC2211599602  // Optional<typename SchemaFrom::FullName>
#define DEFAULT_EVOLUTION_3D54C54053A3933A75E38E133177491491E5DE57D28B91809F26CC2211599602  // Optional<typename SchemaFrom::FullName>
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<SchemaFrom, Optional<typename SchemaFrom::FullName>, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO, typename INTO_TYPE>
  static void Go(const Optional<typename SchemaFrom::FullName>& from, INTO_TYPE& into) {
    if (Exists(from)) {
      typename INTO::FullName evolved;
      Evolve<SchemaFrom, typename SchemaFrom::FullName, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(Value(from), evolved);
      into = evolved;
    } else {
      into = nullptr;
    }
  }
};
#endif

// Default evolution for `Variant<CustomTypeA, CustomTypeB>`.
#ifndef DEFAULT_EVOLUTION_D69FD829A409D52292914AEA28F8388AA8533126BA68719F00BBE4075AF16EAF  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaFrom::CustomTypeA, SchemaFrom::CustomTypeB>>
#define DEFAULT_EVOLUTION_D69FD829A409D52292914AEA28F8388AA8533126BA68719F00BBE4075AF16EAF  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaFrom::CustomTypeA, SchemaFrom::CustomTypeB>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename CURRENT_ACTIVE_EVOLVER>
struct SchemaFrom_ExpandingVariant_Cases {
  DST& into;
  explicit SchemaFrom_ExpandingVariant_Cases(DST& into) : into(into) {}
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
struct Evolve<SchemaFrom, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaFrom::CustomTypeA, SchemaFrom::CustomTypeB>>, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaFrom::CustomTypeA, SchemaFrom::CustomTypeB>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(SchemaFrom_ExpandingVariant_Cases<decltype(into), SchemaFrom, INTO, CURRENT_ACTIVE_EVOLVER>(into));
  }
};
#endif

// Default evolution for `Variant<CustomTypeA, CustomTypeB, CustomTypeC>`.
#ifndef DEFAULT_EVOLUTION_D900DDC6F1C1A2617AC336DD06342B42DE53801AFF9A1A9F50996E1D25B8ABDB  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaFrom::CustomTypeA, SchemaFrom::CustomTypeB, SchemaFrom::CustomTypeC>>
#define DEFAULT_EVOLUTION_D900DDC6F1C1A2617AC336DD06342B42DE53801AFF9A1A9F50996E1D25B8ABDB  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaFrom::CustomTypeA, SchemaFrom::CustomTypeB, SchemaFrom::CustomTypeC>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename CURRENT_ACTIVE_EVOLVER>
struct SchemaFrom_ShrinkingVariant_Cases {
  DST& into;
  explicit SchemaFrom_ShrinkingVariant_Cases(DST& into) : into(into) {}
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
struct Evolve<SchemaFrom, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaFrom::CustomTypeA, SchemaFrom::CustomTypeB, SchemaFrom::CustomTypeC>>, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaFrom::CustomTypeA, SchemaFrom::CustomTypeB, SchemaFrom::CustomTypeC>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(SchemaFrom_ShrinkingVariant_Cases<decltype(into), SchemaFrom, INTO, CURRENT_ACTIVE_EVOLVER>(into));
  }
};
#endif

// Default evolution for `Variant<Basic, FullName, WithOptional, WithExpandingVariant, WithShrinkingVariant, WithFieldsToRemove>`.
#ifndef DEFAULT_EVOLUTION_B30977B54DC1444D5971FC467C49C165B4DD2978B91D2C1DF179E2EE076A457E  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaFrom::Basic, SchemaFrom::FullName, SchemaFrom::WithOptional, SchemaFrom::WithExpandingVariant, SchemaFrom::WithShrinkingVariant, SchemaFrom::WithFieldsToRemove>>
#define DEFAULT_EVOLUTION_B30977B54DC1444D5971FC467C49C165B4DD2978B91D2C1DF179E2EE076A457E  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaFrom::Basic, SchemaFrom::FullName, SchemaFrom::WithOptional, SchemaFrom::WithExpandingVariant, SchemaFrom::WithShrinkingVariant, SchemaFrom::WithFieldsToRemove>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename CURRENT_ACTIVE_EVOLVER>
struct SchemaFrom_All_Cases {
  DST& into;
  explicit SchemaFrom_All_Cases(DST& into) : into(into) {}
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
struct Evolve<SchemaFrom, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaFrom::Basic, SchemaFrom::FullName, SchemaFrom::WithOptional, SchemaFrom::WithExpandingVariant, SchemaFrom::WithShrinkingVariant, SchemaFrom::WithFieldsToRemove>>, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<SchemaFrom::Basic, SchemaFrom::FullName, SchemaFrom::WithOptional, SchemaFrom::WithExpandingVariant, SchemaFrom::WithShrinkingVariant, SchemaFrom::WithFieldsToRemove>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(SchemaFrom_All_Cases<decltype(into), SchemaFrom, INTO, CURRENT_ACTIVE_EVOLVER>(into));
  }
};
#endif

}  // namespace current::type_evolution
}  // namespace current

#if 0  // Boilerplate evolvers.

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaFrom, WithExpandingVariant, {
  CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from.v, into.v);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaFrom, FullName, {
  CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from.first_name, into.first_name);
  CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from.last_name, into.last_name);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaFrom, CustomTypeB, {
  CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from.b, into.b);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaFrom, Basic, {
  CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from.i, into.i);
  CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from.s, into.s);
  CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from.t, into.t);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaFrom, TopLevel, {
  CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from.data, into.data);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaFrom, WithShrinkingVariant, {
  CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from.v, into.v);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaFrom, WithFieldsToRemove, {
  CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from.foo, into.foo);
  CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from.bar, into.bar);
  CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from.baz, into.baz);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaFrom, CustomTypeA, {
  CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from.a, into.a);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaFrom, CustomTypeC, {
  CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from.c, into.c);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaFrom, WithOptional, {
  CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from.maybe_name, into.maybe_name);
});

CURRENT_TYPE_EVOLVER_VARIANT(CustomEvolver, SchemaFrom, t9221067729821834539::ExpandingVariant, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9206911750362052937::CustomTypeA, CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9202573820625447155::CustomTypeB, CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from, into));
};

CURRENT_TYPE_EVOLVER_VARIANT(CustomEvolver, SchemaFrom, t9226317599863657099::ShrinkingVariant, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9206911750362052937::CustomTypeA, CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9202573820625447155::CustomTypeB, CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9207934621170686053::CustomTypeC, CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from, into));
};

CURRENT_TYPE_EVOLVER_VARIANT(CustomEvolver, SchemaFrom, t9227074222822990824::All, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9203341832538601265::Basic, CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9201952458119363219::FullName, CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9209615657629583566::WithOptional, CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9201009161779005289::WithExpandingVariant, CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9205425707876881313::WithShrinkingVariant, CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9206820554754825223::WithFieldsToRemove, CURRENT_NATURAL_EVOLVE(SchemaFrom, CustomDestinationNamespace, from, into));
};

#endif  // Boilerplate evolvers.

// clang-format on
