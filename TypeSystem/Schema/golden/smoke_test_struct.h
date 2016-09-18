// The `current.h` file is the one from `https://github.com/C5T/Current`.
// Compile with `-std=c++11` or higher.

#include "current.h"

// clang-format off

namespace current_userspace {

#ifndef CURRENT_SCHEMA_FOR_T9206969065948310524
#define CURRENT_SCHEMA_FOR_T9206969065948310524
namespace t9206969065948310524 {
CURRENT_STRUCT(Primitives) {
  CURRENT_FIELD(a, uint8_t);
  CURRENT_FIELD_DESCRIPTION(a, "It's the \"order\" of fields that matters.");
  CURRENT_FIELD(b, uint16_t);
  CURRENT_FIELD_DESCRIPTION(b, "Field descriptions can be set in any order.");
  CURRENT_FIELD(c, uint32_t);
  CURRENT_FIELD(d, uint64_t);
  CURRENT_FIELD(e, int8_t);
  CURRENT_FIELD(f, int16_t);
  CURRENT_FIELD(g, int32_t);
  CURRENT_FIELD(h, int64_t);
  CURRENT_FIELD(i, char);
  CURRENT_FIELD(j, std::string);
  CURRENT_FIELD(k, float);
  CURRENT_FIELD(l, double);
  CURRENT_FIELD(m, bool);
  CURRENT_FIELD_DESCRIPTION(m, "Multiline\ndescriptions\ncan be used.");
  CURRENT_FIELD(n, std::chrono::microseconds);
  CURRENT_FIELD(o, std::chrono::milliseconds);
};
}  // namespace t9206969065948310524
#endif  // CURRENT_SCHEMA_FOR_T_9206969065948310524

#ifndef CURRENT_SCHEMA_FOR_T9206911749438269255
#define CURRENT_SCHEMA_FOR_T9206911749438269255
namespace t9206911749438269255 {
CURRENT_STRUCT(A) {
  CURRENT_FIELD(a, int32_t);
};
}  // namespace t9206911749438269255
#endif  // CURRENT_SCHEMA_FOR_T_9206911749438269255

#ifndef CURRENT_SCHEMA_FOR_T9200817599233955266
#define CURRENT_SCHEMA_FOR_T9200817599233955266
namespace t9200817599233955266 {
CURRENT_STRUCT(B, t9206911749438269255::A) {
  CURRENT_FIELD(b, int32_t);
};
}  // namespace t9200817599233955266
#endif  // CURRENT_SCHEMA_FOR_T_9200817599233955266

#ifndef CURRENT_SCHEMA_FOR_T9209827283478105543
#define CURRENT_SCHEMA_FOR_T9209827283478105543
namespace t9209827283478105543 {
CURRENT_STRUCT(B2, t9206911749438269255::A) {
};
}  // namespace t9209827283478105543
#endif  // CURRENT_SCHEMA_FOR_T_9209827283478105543

#ifndef CURRENT_SCHEMA_FOR_T9200000002835747520
#define CURRENT_SCHEMA_FOR_T9200000002835747520
namespace t9200000002835747520 {
CURRENT_STRUCT(Empty) {
};
}  // namespace t9200000002835747520
#endif  // CURRENT_SCHEMA_FOR_T_9200000002835747520

#ifndef CURRENT_SCHEMA_FOR_T9209980946934124423
#define CURRENT_SCHEMA_FOR_T9209980946934124423
namespace t9209980946934124423 {
CURRENT_STRUCT(X) {
  CURRENT_FIELD(x, int32_t);
};
}  // namespace t9209980946934124423
#endif  // CURRENT_SCHEMA_FOR_T_9209980946934124423

#ifndef CURRENT_SCHEMA_FOR_T9010000003568589458
#define CURRENT_SCHEMA_FOR_T9010000003568589458
namespace t9010000003568589458 {
CURRENT_ENUM(E, uint16_t) {};
}  // namespace t9010000003568589458
#endif  // CURRENT_SCHEMA_FOR_T_9010000003568589458

#ifndef CURRENT_SCHEMA_FOR_T9208828720332602574
#define CURRENT_SCHEMA_FOR_T9208828720332602574
namespace t9208828720332602574 {
CURRENT_STRUCT(Y) {
  CURRENT_FIELD(e, t9010000003568589458::E);
};
}  // namespace t9208828720332602574
#endif  // CURRENT_SCHEMA_FOR_T_9208828720332602574

#ifndef CURRENT_SCHEMA_FOR_T9227782344077896555
#define CURRENT_SCHEMA_FOR_T9227782344077896555
namespace t9227782344077896555 {
CURRENT_VARIANT(MyFreakingVariant, t9206911749438269255::A, t9209980946934124423::X, t9208828720332602574::Y);
}  // namespace t9227782344077896555
#endif  // CURRENT_SCHEMA_FOR_T_9227782344077896555

#ifndef CURRENT_SCHEMA_FOR_T9227782347108675041
#define CURRENT_SCHEMA_FOR_T9227782347108675041
namespace t9227782347108675041 {
CURRENT_VARIANT(Variant_B_A_X_Y_E, t9206911749438269255::A, t9209980946934124423::X, t9208828720332602574::Y);
}  // namespace t9227782347108675041
#endif  // CURRENT_SCHEMA_FOR_T_9227782347108675041

#ifndef CURRENT_SCHEMA_FOR_T9202971611369570493
#define CURRENT_SCHEMA_FOR_T9202971611369570493
namespace t9202971611369570493 {
CURRENT_STRUCT(C) {
  CURRENT_FIELD(e, t9200000002835747520::Empty);
  CURRENT_FIELD(c, t9227782344077896555::MyFreakingVariant);
  CURRENT_FIELD(d, t9227782347108675041::Variant_B_A_X_Y_E);
};
}  // namespace t9202971611369570493
#endif  // CURRENT_SCHEMA_FOR_T_9202971611369570493

#ifndef CURRENT_SCHEMA_FOR_T9228482442669086788
#define CURRENT_SCHEMA_FOR_T9228482442669086788
namespace t9228482442669086788 {
CURRENT_VARIANT(Variant_B_A_B_B2_C_Empty_E, t9206911749438269255::A, t9200817599233955266::B, t9209827283478105543::B2, t9202971611369570493::C, t9200000002835747520::Empty);
}  // namespace t9228482442669086788
#endif  // CURRENT_SCHEMA_FOR_T_9228482442669086788

#ifndef CURRENT_SCHEMA_FOR_T9209454265127716773
#define CURRENT_SCHEMA_FOR_T9209454265127716773
namespace t9209454265127716773 {
CURRENT_STRUCT(Templated_T9209980946934124423) {
  CURRENT_FIELD(foo, int32_t);
  CURRENT_FIELD(bar, t9209980946934124423::X);
};
}  // namespace t9209454265127716773
#endif  // CURRENT_SCHEMA_FOR_T_9209454265127716773

#ifndef CURRENT_SCHEMA_FOR_T9209980087718877311
#define CURRENT_SCHEMA_FOR_T9209980087718877311
namespace t9209980087718877311 {
CURRENT_STRUCT(Templated_T9227782344077896555) {
  CURRENT_FIELD(foo, int32_t);
  CURRENT_FIELD(bar, t9227782344077896555::MyFreakingVariant);
};
}  // namespace t9209980087718877311
#endif  // CURRENT_SCHEMA_FOR_T_9209980087718877311

#ifndef CURRENT_SCHEMA_FOR_T9209626390174323094
#define CURRENT_SCHEMA_FOR_T9209626390174323094
namespace t9209626390174323094 {
CURRENT_STRUCT(TemplatedInheriting_T9200000002835747520, t9206911749438269255::A) {
  CURRENT_FIELD(baz, std::string);
  CURRENT_FIELD(meh, t9200000002835747520::Empty);
};
}  // namespace t9209626390174323094
#endif  // CURRENT_SCHEMA_FOR_T_9209626390174323094

#ifndef CURRENT_SCHEMA_FOR_T9200915781714511302
#define CURRENT_SCHEMA_FOR_T9200915781714511302
namespace t9200915781714511302 {
CURRENT_STRUCT(Templated_T9209626390174323094) {
  CURRENT_FIELD(foo, int32_t);
  CURRENT_FIELD(bar, t9209626390174323094::TemplatedInheriting_T9200000002835747520);
};
}  // namespace t9200915781714511302
#endif  // CURRENT_SCHEMA_FOR_T_9200915781714511302

#ifndef CURRENT_SCHEMA_FOR_T9207402181572240291
#define CURRENT_SCHEMA_FOR_T9207402181572240291
namespace t9207402181572240291 {
CURRENT_STRUCT(TemplatedInheriting_T9209980946934124423, t9206911749438269255::A) {
  CURRENT_FIELD(baz, std::string);
  CURRENT_FIELD(meh, t9209980946934124423::X);
};
}  // namespace t9207402181572240291
#endif  // CURRENT_SCHEMA_FOR_T_9207402181572240291

#ifndef CURRENT_SCHEMA_FOR_T9209503190895787129
#define CURRENT_SCHEMA_FOR_T9209503190895787129
namespace t9209503190895787129 {
CURRENT_STRUCT(TemplatedInheriting_T9227782344077896555, t9206911749438269255::A) {
  CURRENT_FIELD(baz, std::string);
  CURRENT_FIELD(meh, t9227782344077896555::MyFreakingVariant);
};
}  // namespace t9209503190895787129
#endif  // CURRENT_SCHEMA_FOR_T_9209503190895787129

#ifndef CURRENT_SCHEMA_FOR_T9201673071807149456
#define CURRENT_SCHEMA_FOR_T9201673071807149456
namespace t9201673071807149456 {
CURRENT_STRUCT(Templated_T9200000002835747520) {
  CURRENT_FIELD(foo, int32_t);
  CURRENT_FIELD(bar, t9200000002835747520::Empty);
};
}  // namespace t9201673071807149456
#endif  // CURRENT_SCHEMA_FOR_T_9201673071807149456

#ifndef CURRENT_SCHEMA_FOR_T9206651538007828258
#define CURRENT_SCHEMA_FOR_T9206651538007828258
namespace t9206651538007828258 {
CURRENT_STRUCT(TemplatedInheriting_T9201673071807149456, t9206911749438269255::A) {
  CURRENT_FIELD(baz, std::string);
  CURRENT_FIELD(meh, t9201673071807149456::Templated_T9200000002835747520);
};
}  // namespace t9206651538007828258
#endif  // CURRENT_SCHEMA_FOR_T_9206651538007828258

#ifndef CURRENT_SCHEMA_FOR_T9204352959449015213
#define CURRENT_SCHEMA_FOR_T9204352959449015213
namespace t9204352959449015213 {
CURRENT_STRUCT(TrickyEvolutionCases) {
  CURRENT_FIELD(o1, Optional<std::string>);
  CURRENT_FIELD(o2, Optional<int32_t>);
  CURRENT_FIELD(o3, Optional<std::vector<std::string>>);
  CURRENT_FIELD(o4, Optional<std::vector<int32_t>>);
  CURRENT_FIELD(o5, Optional<std::vector<t9206911749438269255::A>>);
  CURRENT_FIELD(o6, (std::pair<std::string, Optional<t9206911749438269255::A>>));
  CURRENT_FIELD(o7, (std::map<std::string, Optional<t9206911749438269255::A>>));
};
}  // namespace t9204352959449015213
#endif  // CURRENT_SCHEMA_FOR_T_9204352959449015213

#ifndef CURRENT_SCHEMA_FOR_T9200642690288147741
#define CURRENT_SCHEMA_FOR_T9200642690288147741
namespace t9200642690288147741 {
CURRENT_STRUCT(FullTest) {
  CURRENT_FIELD(primitives, t9206969065948310524::Primitives);
  CURRENT_FIELD_DESCRIPTION(primitives, "A structure with a lot of primitive types.");
  CURRENT_FIELD(v1, std::vector<std::string>);
  CURRENT_FIELD(v2, std::vector<t9206969065948310524::Primitives>);
  CURRENT_FIELD(p, (std::pair<std::string, t9206969065948310524::Primitives>));
  CURRENT_FIELD(o, Optional<t9206969065948310524::Primitives>);
  CURRENT_FIELD(q, t9228482442669086788::Variant_B_A_B_B2_C_Empty_E);
  CURRENT_FIELD_DESCRIPTION(q, "Field | descriptions | FTW !");
  CURRENT_FIELD(w1, t9209454265127716773::Templated_T9209980946934124423);
  CURRENT_FIELD(w2, t9209980087718877311::Templated_T9227782344077896555);
  CURRENT_FIELD(w3, t9200915781714511302::Templated_T9209626390174323094);
  CURRENT_FIELD(w4, t9207402181572240291::TemplatedInheriting_T9209980946934124423);
  CURRENT_FIELD(w5, t9209503190895787129::TemplatedInheriting_T9227782344077896555);
  CURRENT_FIELD(w6, t9206651538007828258::TemplatedInheriting_T9201673071807149456);
  CURRENT_FIELD(tsc, t9204352959449015213::TrickyEvolutionCases);
};
}  // namespace t9200642690288147741
#endif  // CURRENT_SCHEMA_FOR_T_9200642690288147741

}  // namespace current_userspace

#ifndef CURRENT_NAMESPACE_ExposedNamespace_DEFINED
#define CURRENT_NAMESPACE_ExposedNamespace_DEFINED
CURRENT_NAMESPACE(ExposedNamespace) {
  CURRENT_NAMESPACE_TYPE(E, current_userspace::t9010000003568589458::E);
  CURRENT_NAMESPACE_TYPE(Empty, current_userspace::t9200000002835747520::Empty);
  CURRENT_NAMESPACE_TYPE(FullTest, current_userspace::t9200642690288147741::FullTest);
  CURRENT_NAMESPACE_TYPE(B, current_userspace::t9200817599233955266::B);
  CURRENT_NAMESPACE_TYPE(Templated_T9209626390174323094, current_userspace::t9200915781714511302::Templated_T9209626390174323094);
  CURRENT_NAMESPACE_TYPE(Templated_T9200000002835747520, current_userspace::t9201673071807149456::Templated_T9200000002835747520);
  CURRENT_NAMESPACE_TYPE(C, current_userspace::t9202971611369570493::C);
  CURRENT_NAMESPACE_TYPE(TrickyEvolutionCases, current_userspace::t9204352959449015213::TrickyEvolutionCases);
  CURRENT_NAMESPACE_TYPE(TemplatedInheriting_T9201673071807149456, current_userspace::t9206651538007828258::TemplatedInheriting_T9201673071807149456);
  CURRENT_NAMESPACE_TYPE(A, current_userspace::t9206911749438269255::A);
  CURRENT_NAMESPACE_TYPE(Primitives, current_userspace::t9206969065948310524::Primitives);
  CURRENT_NAMESPACE_TYPE(TemplatedInheriting_T9209980946934124423, current_userspace::t9207402181572240291::TemplatedInheriting_T9209980946934124423);
  CURRENT_NAMESPACE_TYPE(Y, current_userspace::t9208828720332602574::Y);
  CURRENT_NAMESPACE_TYPE(Templated_T9209980946934124423, current_userspace::t9209454265127716773::Templated_T9209980946934124423);
  CURRENT_NAMESPACE_TYPE(TemplatedInheriting_T9227782344077896555, current_userspace::t9209503190895787129::TemplatedInheriting_T9227782344077896555);
  CURRENT_NAMESPACE_TYPE(TemplatedInheriting_T9200000002835747520, current_userspace::t9209626390174323094::TemplatedInheriting_T9200000002835747520);
  CURRENT_NAMESPACE_TYPE(B2, current_userspace::t9209827283478105543::B2);
  CURRENT_NAMESPACE_TYPE(Templated_T9227782344077896555, current_userspace::t9209980087718877311::Templated_T9227782344077896555);
  CURRENT_NAMESPACE_TYPE(X, current_userspace::t9209980946934124423::X);
  CURRENT_NAMESPACE_TYPE(MyFreakingVariant, current_userspace::t9227782344077896555::MyFreakingVariant);
  CURRENT_NAMESPACE_TYPE(Variant_B_A_X_Y_E, current_userspace::t9227782347108675041::Variant_B_A_X_Y_E);
  CURRENT_NAMESPACE_TYPE(Variant_B_A_B_B2_C_Empty_E, current_userspace::t9228482442669086788::Variant_B_A_B_B2_C_Empty_E);

  // Privileged types.
  CURRENT_NAMESPACE_TYPE(ExposedEmpty, current_userspace::t9200000002835747520::Empty);
  CURRENT_NAMESPACE_TYPE(ExposedFullTest, current_userspace::t9200642690288147741::FullTest);
  CURRENT_NAMESPACE_TYPE(ExposedPrimitives, current_userspace::t9206969065948310524::Primitives);
};  // CURRENT_NAMESPACE(ExposedNamespace)
#endif  // CURRENT_NAMESPACE_ExposedNamespace_DEFINED

namespace current {
namespace type_evolution {

// Default evolution for `CURRENT_ENUM(E)`.
#ifndef DEFAULT_EVOLUTION_94F245ACBEA5010A5B9FD5444CB3AB40945E9F820F78179AAE8D1F6B1CB083EF  // ExposedNamespace::E
#define DEFAULT_EVOLUTION_94F245ACBEA5010A5B9FD5444CB3AB40945E9F820F78179AAE8D1F6B1CB083EF  // ExposedNamespace::E
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, ExposedNamespace::E, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO>
  static void Go(ExposedNamespace::E from,
                 typename INTO::E& into) {
    into = static_cast<typename INTO::E>(from);
  }
};
#endif

// Default evolution for struct `Empty`.
#ifndef DEFAULT_EVOLUTION_5939C237877725072E3046253DBCC20B9DD887E80C18CE5446104CF0EB2C62C5  // typename ExposedNamespace::Empty
#define DEFAULT_EVOLUTION_5939C237877725072E3046253DBCC20B9DD887E80C18CE5446104CF0EB2C62C5  // typename ExposedNamespace::Empty
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, typename ExposedNamespace::Empty, CURRENT_ACTIVE_EVOLVER> {
  using FROM = ExposedNamespace;
  template <typename INTO>
  static void Go(const typename FROM::Empty& from,
                 typename INTO::Empty& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Empty>::value == 0,
                    "Custom evolver required.");
      static_cast<void>(from);
      static_cast<void>(into);
  }
};
#endif

// Default evolution for struct `FullTest`.
#ifndef DEFAULT_EVOLUTION_59B8F56918FA03C3FF69EDB9C44286B5A17F00C53AD98E9E6C2E2FFDCC9042B8  // typename ExposedNamespace::FullTest
#define DEFAULT_EVOLUTION_59B8F56918FA03C3FF69EDB9C44286B5A17F00C53AD98E9E6C2E2FFDCC9042B8  // typename ExposedNamespace::FullTest
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, typename ExposedNamespace::FullTest, CURRENT_ACTIVE_EVOLVER> {
  using FROM = ExposedNamespace;
  template <typename INTO>
  static void Go(const typename FROM::FullTest& from,
                 typename INTO::FullTest& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::FullTest>::value == 13,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(primitives);
      CURRENT_EVOLVE_FIELD(v1);
      CURRENT_EVOLVE_FIELD(v2);
      CURRENT_EVOLVE_FIELD(p);
      CURRENT_EVOLVE_FIELD(o);
      CURRENT_EVOLVE_FIELD(q);
      CURRENT_EVOLVE_FIELD(w1);
      CURRENT_EVOLVE_FIELD(w2);
      CURRENT_EVOLVE_FIELD(w3);
      CURRENT_EVOLVE_FIELD(w4);
      CURRENT_EVOLVE_FIELD(w5);
      CURRENT_EVOLVE_FIELD(w6);
      CURRENT_EVOLVE_FIELD(tsc);
  }
};
#endif

// Default evolution for struct `B`.
#ifndef DEFAULT_EVOLUTION_A15D5B33561D4874DC860C2ADE32021A400299BED685625855AC7E5C2ACE8B25  // typename ExposedNamespace::B
#define DEFAULT_EVOLUTION_A15D5B33561D4874DC860C2ADE32021A400299BED685625855AC7E5C2ACE8B25  // typename ExposedNamespace::B
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, typename ExposedNamespace::B, CURRENT_ACTIVE_EVOLVER> {
  using FROM = ExposedNamespace;
  template <typename INTO>
  static void Go(const typename FROM::B& from,
                 typename INTO::B& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::B>::value == 1,
                    "Custom evolver required.");
      Evolve<FROM, FROM::A, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(static_cast<const typename FROM::A&>(from), static_cast<typename INTO::A&>(into));
      CURRENT_EVOLVE_FIELD(b);
  }
};
#endif

// Default evolution for struct `Templated_T9209626390174323094`.
#ifndef DEFAULT_EVOLUTION_9066C275D8288ED3744F89BF3B0474B04AAE1571E60619068250ED037D4CFBC7  // typename ExposedNamespace::Templated_T9209626390174323094
#define DEFAULT_EVOLUTION_9066C275D8288ED3744F89BF3B0474B04AAE1571E60619068250ED037D4CFBC7  // typename ExposedNamespace::Templated_T9209626390174323094
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, typename ExposedNamespace::Templated_T9209626390174323094, CURRENT_ACTIVE_EVOLVER> {
  using FROM = ExposedNamespace;
  template <typename INTO>
  static void Go(const typename FROM::Templated_T9209626390174323094& from,
                 typename INTO::Templated_T9209626390174323094& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Templated_T9209626390174323094>::value == 2,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(foo);
      CURRENT_EVOLVE_FIELD(bar);
  }
};
#endif

// Default evolution for struct `Templated_T9200000002835747520`.
#ifndef DEFAULT_EVOLUTION_EA77C70DF8F4BE40294BFD6B28B1BD23185E3A99F196BF933481EFE294A3403F  // typename ExposedNamespace::Templated_T9200000002835747520
#define DEFAULT_EVOLUTION_EA77C70DF8F4BE40294BFD6B28B1BD23185E3A99F196BF933481EFE294A3403F  // typename ExposedNamespace::Templated_T9200000002835747520
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, typename ExposedNamespace::Templated_T9200000002835747520, CURRENT_ACTIVE_EVOLVER> {
  using FROM = ExposedNamespace;
  template <typename INTO>
  static void Go(const typename FROM::Templated_T9200000002835747520& from,
                 typename INTO::Templated_T9200000002835747520& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Templated_T9200000002835747520>::value == 2,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(foo);
      CURRENT_EVOLVE_FIELD(bar);
  }
};
#endif

// Default evolution for struct `C`.
#ifndef DEFAULT_EVOLUTION_DDE310AA7719296DBACC18106A71460781CAEEE2B04F24A1109BB0B94167270F  // typename ExposedNamespace::C
#define DEFAULT_EVOLUTION_DDE310AA7719296DBACC18106A71460781CAEEE2B04F24A1109BB0B94167270F  // typename ExposedNamespace::C
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, typename ExposedNamespace::C, CURRENT_ACTIVE_EVOLVER> {
  using FROM = ExposedNamespace;
  template <typename INTO>
  static void Go(const typename FROM::C& from,
                 typename INTO::C& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::C>::value == 3,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(e);
      CURRENT_EVOLVE_FIELD(c);
      CURRENT_EVOLVE_FIELD(d);
  }
};
#endif

// Default evolution for struct `TrickyEvolutionCases`.
#ifndef DEFAULT_EVOLUTION_49C764EB5BE1119C7F7A682FED912AC6F84C1B7E50DE5FFD2C6B4BF92F8627DC  // typename ExposedNamespace::TrickyEvolutionCases
#define DEFAULT_EVOLUTION_49C764EB5BE1119C7F7A682FED912AC6F84C1B7E50DE5FFD2C6B4BF92F8627DC  // typename ExposedNamespace::TrickyEvolutionCases
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, typename ExposedNamespace::TrickyEvolutionCases, CURRENT_ACTIVE_EVOLVER> {
  using FROM = ExposedNamespace;
  template <typename INTO>
  static void Go(const typename FROM::TrickyEvolutionCases& from,
                 typename INTO::TrickyEvolutionCases& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TrickyEvolutionCases>::value == 7,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(o1);
      CURRENT_EVOLVE_FIELD(o2);
      CURRENT_EVOLVE_FIELD(o3);
      CURRENT_EVOLVE_FIELD(o4);
      CURRENT_EVOLVE_FIELD(o5);
      CURRENT_EVOLVE_FIELD(o6);
      CURRENT_EVOLVE_FIELD(o7);
  }
};
#endif

// Default evolution for struct `TemplatedInheriting_T9201673071807149456`.
#ifndef DEFAULT_EVOLUTION_861EF512C56B8CAB7389A30E16021A83F518B8363C2D9DA113A11B613A5BA0E4  // typename ExposedNamespace::TemplatedInheriting_T9201673071807149456
#define DEFAULT_EVOLUTION_861EF512C56B8CAB7389A30E16021A83F518B8363C2D9DA113A11B613A5BA0E4  // typename ExposedNamespace::TemplatedInheriting_T9201673071807149456
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, typename ExposedNamespace::TemplatedInheriting_T9201673071807149456, CURRENT_ACTIVE_EVOLVER> {
  using FROM = ExposedNamespace;
  template <typename INTO>
  static void Go(const typename FROM::TemplatedInheriting_T9201673071807149456& from,
                 typename INTO::TemplatedInheriting_T9201673071807149456& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TemplatedInheriting_T9201673071807149456>::value == 2,
                    "Custom evolver required.");
      Evolve<FROM, FROM::A, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(static_cast<const typename FROM::A&>(from), static_cast<typename INTO::A&>(into));
      CURRENT_EVOLVE_FIELD(baz);
      CURRENT_EVOLVE_FIELD(meh);
  }
};
#endif

// Default evolution for struct `A`.
#ifndef DEFAULT_EVOLUTION_CB2E976118E62268E31533B2FCB4ACDC7D423A3F3C999C40A059D3CE7069663A  // typename ExposedNamespace::A
#define DEFAULT_EVOLUTION_CB2E976118E62268E31533B2FCB4ACDC7D423A3F3C999C40A059D3CE7069663A  // typename ExposedNamespace::A
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, typename ExposedNamespace::A, CURRENT_ACTIVE_EVOLVER> {
  using FROM = ExposedNamespace;
  template <typename INTO>
  static void Go(const typename FROM::A& from,
                 typename INTO::A& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::A>::value == 1,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(a);
  }
};
#endif

// Default evolution for struct `Primitives`.
#ifndef DEFAULT_EVOLUTION_2939885D492B19EC612443266E33601C2D5E89FA766AD88BACC05022A1C6BD00  // typename ExposedNamespace::Primitives
#define DEFAULT_EVOLUTION_2939885D492B19EC612443266E33601C2D5E89FA766AD88BACC05022A1C6BD00  // typename ExposedNamespace::Primitives
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, typename ExposedNamespace::Primitives, CURRENT_ACTIVE_EVOLVER> {
  using FROM = ExposedNamespace;
  template <typename INTO>
  static void Go(const typename FROM::Primitives& from,
                 typename INTO::Primitives& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Primitives>::value == 15,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(a);
      CURRENT_EVOLVE_FIELD(b);
      CURRENT_EVOLVE_FIELD(c);
      CURRENT_EVOLVE_FIELD(d);
      CURRENT_EVOLVE_FIELD(e);
      CURRENT_EVOLVE_FIELD(f);
      CURRENT_EVOLVE_FIELD(g);
      CURRENT_EVOLVE_FIELD(h);
      CURRENT_EVOLVE_FIELD(i);
      CURRENT_EVOLVE_FIELD(j);
      CURRENT_EVOLVE_FIELD(k);
      CURRENT_EVOLVE_FIELD(l);
      CURRENT_EVOLVE_FIELD(m);
      CURRENT_EVOLVE_FIELD(n);
      CURRENT_EVOLVE_FIELD(o);
  }
};
#endif

// Default evolution for struct `TemplatedInheriting_T9209980946934124423`.
#ifndef DEFAULT_EVOLUTION_A8EDEC803A73757F6D0E8DD935C763F066E9B9193AB7D1E153B1A8D784804DCC  // typename ExposedNamespace::TemplatedInheriting_T9209980946934124423
#define DEFAULT_EVOLUTION_A8EDEC803A73757F6D0E8DD935C763F066E9B9193AB7D1E153B1A8D784804DCC  // typename ExposedNamespace::TemplatedInheriting_T9209980946934124423
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, typename ExposedNamespace::TemplatedInheriting_T9209980946934124423, CURRENT_ACTIVE_EVOLVER> {
  using FROM = ExposedNamespace;
  template <typename INTO>
  static void Go(const typename FROM::TemplatedInheriting_T9209980946934124423& from,
                 typename INTO::TemplatedInheriting_T9209980946934124423& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TemplatedInheriting_T9209980946934124423>::value == 2,
                    "Custom evolver required.");
      Evolve<FROM, FROM::A, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(static_cast<const typename FROM::A&>(from), static_cast<typename INTO::A&>(into));
      CURRENT_EVOLVE_FIELD(baz);
      CURRENT_EVOLVE_FIELD(meh);
  }
};
#endif

// Default evolution for struct `Y`.
#ifndef DEFAULT_EVOLUTION_BF73E632C3E758DE2753A63B99D9BBC9BFB0AA2293FC634374C6A76DF21386CE  // typename ExposedNamespace::Y
#define DEFAULT_EVOLUTION_BF73E632C3E758DE2753A63B99D9BBC9BFB0AA2293FC634374C6A76DF21386CE  // typename ExposedNamespace::Y
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, typename ExposedNamespace::Y, CURRENT_ACTIVE_EVOLVER> {
  using FROM = ExposedNamespace;
  template <typename INTO>
  static void Go(const typename FROM::Y& from,
                 typename INTO::Y& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Y>::value == 1,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(e);
  }
};
#endif

// Default evolution for struct `Templated_T9209980946934124423`.
#ifndef DEFAULT_EVOLUTION_A88B17CDDDFDDDE6DA4AD1A5060172098C276373D45EAD8F379CB84FA882A590  // typename ExposedNamespace::Templated_T9209980946934124423
#define DEFAULT_EVOLUTION_A88B17CDDDFDDDE6DA4AD1A5060172098C276373D45EAD8F379CB84FA882A590  // typename ExposedNamespace::Templated_T9209980946934124423
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, typename ExposedNamespace::Templated_T9209980946934124423, CURRENT_ACTIVE_EVOLVER> {
  using FROM = ExposedNamespace;
  template <typename INTO>
  static void Go(const typename FROM::Templated_T9209980946934124423& from,
                 typename INTO::Templated_T9209980946934124423& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Templated_T9209980946934124423>::value == 2,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(foo);
      CURRENT_EVOLVE_FIELD(bar);
  }
};
#endif

// Default evolution for struct `TemplatedInheriting_T9227782344077896555`.
#ifndef DEFAULT_EVOLUTION_BF9F7F4895FE4B3D53F5332610604599A08A6686DAFA8EA57DB6D0B3995A3A05  // typename ExposedNamespace::TemplatedInheriting_T9227782344077896555
#define DEFAULT_EVOLUTION_BF9F7F4895FE4B3D53F5332610604599A08A6686DAFA8EA57DB6D0B3995A3A05  // typename ExposedNamespace::TemplatedInheriting_T9227782344077896555
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, typename ExposedNamespace::TemplatedInheriting_T9227782344077896555, CURRENT_ACTIVE_EVOLVER> {
  using FROM = ExposedNamespace;
  template <typename INTO>
  static void Go(const typename FROM::TemplatedInheriting_T9227782344077896555& from,
                 typename INTO::TemplatedInheriting_T9227782344077896555& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TemplatedInheriting_T9227782344077896555>::value == 2,
                    "Custom evolver required.");
      Evolve<FROM, FROM::A, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(static_cast<const typename FROM::A&>(from), static_cast<typename INTO::A&>(into));
      CURRENT_EVOLVE_FIELD(baz);
      CURRENT_EVOLVE_FIELD(meh);
  }
};
#endif

// Default evolution for struct `TemplatedInheriting_T9200000002835747520`.
#ifndef DEFAULT_EVOLUTION_C5ABE688A04E351A29474634960FB1B4723A5E6BFD6F6C6BA3F085843D540AD3  // typename ExposedNamespace::TemplatedInheriting_T9200000002835747520
#define DEFAULT_EVOLUTION_C5ABE688A04E351A29474634960FB1B4723A5E6BFD6F6C6BA3F085843D540AD3  // typename ExposedNamespace::TemplatedInheriting_T9200000002835747520
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, typename ExposedNamespace::TemplatedInheriting_T9200000002835747520, CURRENT_ACTIVE_EVOLVER> {
  using FROM = ExposedNamespace;
  template <typename INTO>
  static void Go(const typename FROM::TemplatedInheriting_T9200000002835747520& from,
                 typename INTO::TemplatedInheriting_T9200000002835747520& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TemplatedInheriting_T9200000002835747520>::value == 2,
                    "Custom evolver required.");
      Evolve<FROM, FROM::A, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(static_cast<const typename FROM::A&>(from), static_cast<typename INTO::A&>(into));
      CURRENT_EVOLVE_FIELD(baz);
      CURRENT_EVOLVE_FIELD(meh);
  }
};
#endif

// Default evolution for struct `B2`.
#ifndef DEFAULT_EVOLUTION_8A1E7F884F5E71838181FED05934EFF1984E0B1C0F84A50B298E7A662E76C7C3  // typename ExposedNamespace::B2
#define DEFAULT_EVOLUTION_8A1E7F884F5E71838181FED05934EFF1984E0B1C0F84A50B298E7A662E76C7C3  // typename ExposedNamespace::B2
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, typename ExposedNamespace::B2, CURRENT_ACTIVE_EVOLVER> {
  using FROM = ExposedNamespace;
  template <typename INTO>
  static void Go(const typename FROM::B2& from,
                 typename INTO::B2& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::B2>::value == 0,
                    "Custom evolver required.");
      Evolve<FROM, FROM::A, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(static_cast<const typename FROM::A&>(from), static_cast<typename INTO::A&>(into));
      static_cast<void>(from);
      static_cast<void>(into);
  }
};
#endif

// Default evolution for struct `Templated_T9227782344077896555`.
#ifndef DEFAULT_EVOLUTION_E85C06F34A2C944B948194E1C7BA0B4F17EFC2E450488D292B66702728F3AE25  // typename ExposedNamespace::Templated_T9227782344077896555
#define DEFAULT_EVOLUTION_E85C06F34A2C944B948194E1C7BA0B4F17EFC2E450488D292B66702728F3AE25  // typename ExposedNamespace::Templated_T9227782344077896555
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, typename ExposedNamespace::Templated_T9227782344077896555, CURRENT_ACTIVE_EVOLVER> {
  using FROM = ExposedNamespace;
  template <typename INTO>
  static void Go(const typename FROM::Templated_T9227782344077896555& from,
                 typename INTO::Templated_T9227782344077896555& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Templated_T9227782344077896555>::value == 2,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(foo);
      CURRENT_EVOLVE_FIELD(bar);
  }
};
#endif

// Default evolution for struct `X`.
#ifndef DEFAULT_EVOLUTION_6A49BB9E6B1311D2427D567AF16667E71D185DC267636B6BBBC05421E48C061B  // typename ExposedNamespace::X
#define DEFAULT_EVOLUTION_6A49BB9E6B1311D2427D567AF16667E71D185DC267636B6BBBC05421E48C061B  // typename ExposedNamespace::X
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, typename ExposedNamespace::X, CURRENT_ACTIVE_EVOLVER> {
  using FROM = ExposedNamespace;
  template <typename INTO>
  static void Go(const typename FROM::X& from,
                 typename INTO::X& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::X>::value == 1,
                    "Custom evolver required.");
      CURRENT_EVOLVE_FIELD(x);
  }
};
#endif

// Default evolution for `Optional<t9206911749438269255::A>`.
#ifndef DEFAULT_EVOLUTION_335EF7A9E1BEA2104FA53AD08270A403897AA5A2AC3947337BB7EA4D8652D2D5  // Optional<typename ExposedNamespace::A>
#define DEFAULT_EVOLUTION_335EF7A9E1BEA2104FA53AD08270A403897AA5A2AC3947337BB7EA4D8652D2D5  // Optional<typename ExposedNamespace::A>
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, Optional<typename ExposedNamespace::A>, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO, typename INTO_TYPE>
  static void Go(const Optional<typename ExposedNamespace::A>& from, INTO_TYPE& into) {
    if (Exists(from)) {
      typename INTO::A evolved;
      Evolve<ExposedNamespace, typename ExposedNamespace::A, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(Value(from), evolved);
      into = evolved;
    } else {
      into = nullptr;
    }
  }
};
#endif

// Default evolution for `Optional<t9206969065948310524::Primitives>`.
#ifndef DEFAULT_EVOLUTION_BE977B90D0AA48F4FC3D7A76F5C8E203C2C740EEC1E87137CEE976422C74B0D5  // Optional<typename ExposedNamespace::Primitives>
#define DEFAULT_EVOLUTION_BE977B90D0AA48F4FC3D7A76F5C8E203C2C740EEC1E87137CEE976422C74B0D5  // Optional<typename ExposedNamespace::Primitives>
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, Optional<typename ExposedNamespace::Primitives>, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO, typename INTO_TYPE>
  static void Go(const Optional<typename ExposedNamespace::Primitives>& from, INTO_TYPE& into) {
    if (Exists(from)) {
      typename INTO::Primitives evolved;
      Evolve<ExposedNamespace, typename ExposedNamespace::Primitives, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(Value(from), evolved);
      into = evolved;
    } else {
      into = nullptr;
    }
  }
};
#endif

// Default evolution for `Optional<std::vector<t9206911749438269255::A>>`.
#ifndef DEFAULT_EVOLUTION_4BC8B7225437989E35C9EB7F62A7AE79E09299AE29CBCAE1035894375DD60018  // Optional<std::vector<typename ExposedNamespace::A>>
#define DEFAULT_EVOLUTION_4BC8B7225437989E35C9EB7F62A7AE79E09299AE29CBCAE1035894375DD60018  // Optional<std::vector<typename ExposedNamespace::A>>
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, Optional<std::vector<typename ExposedNamespace::A>>, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO, typename INTO_TYPE>
  static void Go(const Optional<std::vector<typename ExposedNamespace::A>>& from, INTO_TYPE& into) {
    if (Exists(from)) {
      std::vector<typename INTO::A> evolved;
      Evolve<ExposedNamespace, std::vector<typename ExposedNamespace::A>, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(Value(from), evolved);
      into = evolved;
    } else {
      into = nullptr;
    }
  }
};
#endif

// Default evolution for `Optional<std::vector<int32_t>>`.
#ifndef DEFAULT_EVOLUTION_F842514CCF3605B350AF7450C3D994B0FC6982CA714F7B8924A37016DCF7D5C1  // Optional<std::vector<int32_t>>
#define DEFAULT_EVOLUTION_F842514CCF3605B350AF7450C3D994B0FC6982CA714F7B8924A37016DCF7D5C1  // Optional<std::vector<int32_t>>
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, Optional<std::vector<int32_t>>, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO, typename INTO_TYPE>
  static void Go(const Optional<std::vector<int32_t>>& from, INTO_TYPE& into) {
    if (Exists(from)) {
      std::vector<int32_t> evolved;
      Evolve<ExposedNamespace, std::vector<int32_t>, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(Value(from), evolved);
      into = evolved;
    } else {
      into = nullptr;
    }
  }
};
#endif

// Default evolution for `Optional<std::vector<std::string>>`.
#ifndef DEFAULT_EVOLUTION_FA2394B37D58C5EB952B0A685E82FF630767B7DE653B4ED5ECABABFB8A3AF0BB  // Optional<std::vector<std::string>>
#define DEFAULT_EVOLUTION_FA2394B37D58C5EB952B0A685E82FF630767B7DE653B4ED5ECABABFB8A3AF0BB  // Optional<std::vector<std::string>>
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedNamespace, Optional<std::vector<std::string>>, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO, typename INTO_TYPE>
  static void Go(const Optional<std::vector<std::string>>& from, INTO_TYPE& into) {
    if (Exists(from)) {
      std::vector<std::string> evolved;
      Evolve<ExposedNamespace, std::vector<std::string>, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(Value(from), evolved);
      into = evolved;
    } else {
      into = nullptr;
    }
  }
};
#endif

// Default evolution for `Variant<A, X, Y>`.
#ifndef DEFAULT_EVOLUTION_07232405E57CFE405500B20AE4462E6416EAC17487EFF5AA64EFC9D7A195BF82  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<ExposedNamespace::A, ExposedNamespace::X, ExposedNamespace::Y>>
#define DEFAULT_EVOLUTION_07232405E57CFE405500B20AE4462E6416EAC17487EFF5AA64EFC9D7A195BF82  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<ExposedNamespace::A, ExposedNamespace::X, ExposedNamespace::Y>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename CURRENT_ACTIVE_EVOLVER>
struct ExposedNamespace_MyFreakingVariant_Cases {
  DST& into;
  explicit ExposedNamespace_MyFreakingVariant_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::A& value) const {
    using into_t = typename INTO::A;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::A, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::X& value) const {
    using into_t = typename INTO::X;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::X, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::Y& value) const {
    using into_t = typename INTO::Y;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::Y, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename CURRENT_ACTIVE_EVOLVER, typename VARIANT_NAME_HELPER>
struct Evolve<ExposedNamespace, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<ExposedNamespace::A, ExposedNamespace::X, ExposedNamespace::Y>>, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<ExposedNamespace::A, ExposedNamespace::X, ExposedNamespace::Y>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(ExposedNamespace_MyFreakingVariant_Cases<decltype(into), ExposedNamespace, INTO, CURRENT_ACTIVE_EVOLVER>(into));
  }
};
#endif

// Default evolution for `Variant<A, B, B2, C, Empty>`.
#ifndef DEFAULT_EVOLUTION_34D2032062E09B23986AD3F4B8DCF2784A70169CE43B2E9AF44303A8D5A3A2D0  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<ExposedNamespace::A, ExposedNamespace::B, ExposedNamespace::B2, ExposedNamespace::C, ExposedNamespace::Empty>>
#define DEFAULT_EVOLUTION_34D2032062E09B23986AD3F4B8DCF2784A70169CE43B2E9AF44303A8D5A3A2D0  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<ExposedNamespace::A, ExposedNamespace::B, ExposedNamespace::B2, ExposedNamespace::C, ExposedNamespace::Empty>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename CURRENT_ACTIVE_EVOLVER>
struct ExposedNamespace_Variant_B_A_B_B2_C_Empty_E_Cases {
  DST& into;
  explicit ExposedNamespace_Variant_B_A_B_B2_C_Empty_E_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::A& value) const {
    using into_t = typename INTO::A;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::A, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::B& value) const {
    using into_t = typename INTO::B;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::B, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::B2& value) const {
    using into_t = typename INTO::B2;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::B2, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::C& value) const {
    using into_t = typename INTO::C;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::C, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::Empty& value) const {
    using into_t = typename INTO::Empty;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::Empty, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename CURRENT_ACTIVE_EVOLVER, typename VARIANT_NAME_HELPER>
struct Evolve<ExposedNamespace, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<ExposedNamespace::A, ExposedNamespace::B, ExposedNamespace::B2, ExposedNamespace::C, ExposedNamespace::Empty>>, CURRENT_ACTIVE_EVOLVER> {
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<ExposedNamespace::A, ExposedNamespace::B, ExposedNamespace::B2, ExposedNamespace::C, ExposedNamespace::Empty>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(ExposedNamespace_Variant_B_A_B_B2_C_Empty_E_Cases<decltype(into), ExposedNamespace, INTO, CURRENT_ACTIVE_EVOLVER>(into));
  }
};
#endif

}  // namespace current::type_evolution
}  // namespace current

#if 0  // Boilerplate evolvers.

CURRENT_TYPE_EVOLVER(CustomEvolver, ExposedNamespace, Empty, {
});

CURRENT_TYPE_EVOLVER(CustomEvolver, ExposedNamespace, FullTest, {
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.primitives, into.primitives);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.v1, into.v1);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.v2, into.v2);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.p, into.p);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.o, into.o);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.q, into.q);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.w1, into.w1);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.w2, into.w2);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.w3, into.w3);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.w4, into.w4);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.w5, into.w5);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.w6, into.w6);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.tsc, into.tsc);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, ExposedNamespace, B, {
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.b, into.b);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, ExposedNamespace, Templated_T9209626390174323094, {
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.foo, into.foo);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.bar, into.bar);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, ExposedNamespace, Templated_T9200000002835747520, {
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.foo, into.foo);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.bar, into.bar);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, ExposedNamespace, C, {
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.e, into.e);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.c, into.c);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.d, into.d);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, ExposedNamespace, TrickyEvolutionCases, {
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.o1, into.o1);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.o2, into.o2);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.o3, into.o3);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.o4, into.o4);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.o5, into.o5);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.o6, into.o6);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.o7, into.o7);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, ExposedNamespace, TemplatedInheriting_T9201673071807149456, {
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.baz, into.baz);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.meh, into.meh);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, ExposedNamespace, A, {
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.a, into.a);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, ExposedNamespace, Primitives, {
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.a, into.a);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.b, into.b);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.c, into.c);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.d, into.d);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.e, into.e);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.f, into.f);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.g, into.g);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.h, into.h);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.i, into.i);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.j, into.j);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.k, into.k);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.l, into.l);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.m, into.m);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.n, into.n);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.o, into.o);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, ExposedNamespace, TemplatedInheriting_T9209980946934124423, {
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.baz, into.baz);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.meh, into.meh);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, ExposedNamespace, Y, {
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.e, into.e);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, ExposedNamespace, Templated_T9209980946934124423, {
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.foo, into.foo);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.bar, into.bar);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, ExposedNamespace, TemplatedInheriting_T9227782344077896555, {
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.baz, into.baz);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.meh, into.meh);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, ExposedNamespace, TemplatedInheriting_T9200000002835747520, {
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.baz, into.baz);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.meh, into.meh);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, ExposedNamespace, B2, {
});

CURRENT_TYPE_EVOLVER(CustomEvolver, ExposedNamespace, Templated_T9227782344077896555, {
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.foo, into.foo);
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.bar, into.bar);
});

CURRENT_TYPE_EVOLVER(CustomEvolver, ExposedNamespace, X, {
  CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from.x, into.x);
});

CURRENT_TYPE_EVOLVER_VARIANT(CustomEvolver, ExposedNamespace, t9227782344077896555::MyFreakingVariant, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9206911749438269255::A, CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9209980946934124423::X, CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9208828720332602574::Y, CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from, into));
};

CURRENT_TYPE_EVOLVER_VARIANT(CustomEvolver, ExposedNamespace, t9227782347108675041::Variant_B_A_X_Y_E, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9206911749438269255::A, CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9209980946934124423::X, CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9208828720332602574::Y, CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from, into));
};

CURRENT_TYPE_EVOLVER_VARIANT(CustomEvolver, ExposedNamespace, t9228482442669086788::Variant_B_A_B_B2_C_Empty_E, CustomDestinationNamespace) {
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9206911749438269255::A, CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9200817599233955266::B, CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9209827283478105543::B2, CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9202971611369570493::C, CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from, into));
  CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(t9200000002835747520::Empty, CURRENT_NATURAL_EVOLVE(ExposedNamespace, CustomDestinationNamespace, from, into));
};

#endif  // Boilerplate evolvers.

// clang-format on
