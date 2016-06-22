// The `current.h` file is the one from `https://github.com/C5T/Current`.
// Compile with `-std=c++11` or higher.

#ifndef CURRENT_USERSPACE_B528C6626C40443D
#define CURRENT_USERSPACE_B528C6626C40443D

#include "current.h"

// clang-format off

namespace current_userspace_b528c6626c40443d {

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
using T9206969065948310524 = Primitives;

CURRENT_STRUCT(A) {
  CURRENT_FIELD(a, int32_t);
};
using T9206911749438269255 = A;

CURRENT_STRUCT(B, A) {
  CURRENT_FIELD(b, int32_t);
};
using T9200817599233955266 = B;

CURRENT_STRUCT(B2, A) {
};
using T9209827283478105543 = B2;

CURRENT_STRUCT(Empty) {
};
using T9200000002835747520 = Empty;

CURRENT_STRUCT(X) {
  CURRENT_FIELD(x, int32_t);
};
using T9209980946934124423 = X;

CURRENT_ENUM(E, uint16_t) {};
using T9010000003568589458 = E;

CURRENT_STRUCT(Y) {
  CURRENT_FIELD(e, E);
};
using T9208828720332602574 = Y;

CURRENT_VARIANT(MyFreakingVariant, A, X, Y);
using T9227782344077896555 = MyFreakingVariant;

CURRENT_VARIANT(Variant_B_A_X_Y_E, A, X, Y);
using T9227782347108675041 = Variant_B_A_X_Y_E;

CURRENT_STRUCT(C) {
  CURRENT_FIELD(e, Empty);
  CURRENT_FIELD(c, MyFreakingVariant);
  CURRENT_FIELD(d, Variant_B_A_X_Y_E);
};
using T9202971611369570493 = C;

CURRENT_VARIANT(Variant_B_A_B_B2_C_Empty_E, A, B, B2, C, Empty);
using T9228482442669086788 = Variant_B_A_B_B2_C_Empty_E;

CURRENT_STRUCT(Templated_T9209980946934124423) {
  CURRENT_FIELD(foo, int32_t);
  CURRENT_FIELD(bar, X);
};
using T9209454265127716773 = Templated_T9209980946934124423;

CURRENT_STRUCT(Templated_T9227782344077896555) {
  CURRENT_FIELD(foo, int32_t);
  CURRENT_FIELD(bar, MyFreakingVariant);
};
using T9209980087718877311 = Templated_T9227782344077896555;

CURRENT_STRUCT(TemplatedInheriting_T9200000002835747520, A) {
  CURRENT_FIELD(baz, std::string);
  CURRENT_FIELD(meh, Empty);
};
using T9209626390174323094 = TemplatedInheriting_T9200000002835747520;

CURRENT_STRUCT(Templated_T9209626390174323094) {
  CURRENT_FIELD(foo, int32_t);
  CURRENT_FIELD(bar, TemplatedInheriting_T9200000002835747520);
};
using T9200915781714511302 = Templated_T9209626390174323094;

CURRENT_STRUCT(TemplatedInheriting_T9209980946934124423, A) {
  CURRENT_FIELD(baz, std::string);
  CURRENT_FIELD(meh, X);
};
using T9207402181572240291 = TemplatedInheriting_T9209980946934124423;

CURRENT_STRUCT(TemplatedInheriting_T9227782344077896555, A) {
  CURRENT_FIELD(baz, std::string);
  CURRENT_FIELD(meh, MyFreakingVariant);
};
using T9209503190895787129 = TemplatedInheriting_T9227782344077896555;

CURRENT_STRUCT(Templated_T9200000002835747520) {
  CURRENT_FIELD(foo, int32_t);
  CURRENT_FIELD(bar, Empty);
};
using T9201673071807149456 = Templated_T9200000002835747520;

CURRENT_STRUCT(TemplatedInheriting_T9201673071807149456, A) {
  CURRENT_FIELD(baz, std::string);
  CURRENT_FIELD(meh, Templated_T9200000002835747520);
};
using T9206651538007828258 = TemplatedInheriting_T9201673071807149456;

CURRENT_STRUCT(TrickyEvolutionCases) {
  CURRENT_FIELD(o1, Optional<std::string>);
  CURRENT_FIELD(o2, Optional<int32_t>);
  CURRENT_FIELD(o3, Optional<std::vector<std::string>>);
  CURRENT_FIELD(o4, Optional<std::vector<int32_t>>);
  CURRENT_FIELD(o5, Optional<std::vector<A>>);
  CURRENT_FIELD(o6, (std::pair<std::string, Optional<A>>));
  CURRENT_FIELD(o7, (std::map<std::string, Optional<A>>));
};
using T9204352959449015213 = TrickyEvolutionCases;

CURRENT_STRUCT(FullTest) {
  CURRENT_FIELD(primitives, Primitives);
  CURRENT_FIELD_DESCRIPTION(primitives, "A structure with a lot of primitive types.");
  CURRENT_FIELD(v1, std::vector<std::string>);
  CURRENT_FIELD(v2, std::vector<Primitives>);
  CURRENT_FIELD(p, (std::pair<std::string, Primitives>));
  CURRENT_FIELD(o, Optional<Primitives>);
  CURRENT_FIELD(q, Variant_B_A_B_B2_C_Empty_E);
  CURRENT_FIELD_DESCRIPTION(q, "Field | descriptions | FTW !");
  CURRENT_FIELD(w1, Templated_T9209980946934124423);
  CURRENT_FIELD(w2, Templated_T9227782344077896555);
  CURRENT_FIELD(w3, Templated_T9209626390174323094);
  CURRENT_FIELD(w4, TemplatedInheriting_T9209980946934124423);
  CURRENT_FIELD(w5, TemplatedInheriting_T9227782344077896555);
  CURRENT_FIELD(w6, TemplatedInheriting_T9201673071807149456);
  CURRENT_FIELD(tsc, TrickyEvolutionCases);
};
using T9200642690288147741 = FullTest;

}  // namespace current_userspace_b528c6626c40443d

CURRENT_NAMESPACE(USERSPACE_B528C6626C40443D) {
  CURRENT_NAMESPACE_TYPE(E, current_userspace_b528c6626c40443d::E);
  CURRENT_NAMESPACE_TYPE(Empty, current_userspace_b528c6626c40443d::Empty);
  CURRENT_NAMESPACE_TYPE(FullTest, current_userspace_b528c6626c40443d::FullTest);
  CURRENT_NAMESPACE_TYPE(B, current_userspace_b528c6626c40443d::B);
  CURRENT_NAMESPACE_TYPE(Templated_T9209626390174323094, current_userspace_b528c6626c40443d::Templated_T9209626390174323094);
  CURRENT_NAMESPACE_TYPE(Templated_T9200000002835747520, current_userspace_b528c6626c40443d::Templated_T9200000002835747520);
  CURRENT_NAMESPACE_TYPE(C, current_userspace_b528c6626c40443d::C);
  CURRENT_NAMESPACE_TYPE(TrickyEvolutionCases, current_userspace_b528c6626c40443d::TrickyEvolutionCases);
  CURRENT_NAMESPACE_TYPE(TemplatedInheriting_T9201673071807149456, current_userspace_b528c6626c40443d::TemplatedInheriting_T9201673071807149456);
  CURRENT_NAMESPACE_TYPE(A, current_userspace_b528c6626c40443d::A);
  CURRENT_NAMESPACE_TYPE(Primitives, current_userspace_b528c6626c40443d::Primitives);
  CURRENT_NAMESPACE_TYPE(TemplatedInheriting_T9209980946934124423, current_userspace_b528c6626c40443d::TemplatedInheriting_T9209980946934124423);
  CURRENT_NAMESPACE_TYPE(Y, current_userspace_b528c6626c40443d::Y);
  CURRENT_NAMESPACE_TYPE(Templated_T9209980946934124423, current_userspace_b528c6626c40443d::Templated_T9209980946934124423);
  CURRENT_NAMESPACE_TYPE(TemplatedInheriting_T9227782344077896555, current_userspace_b528c6626c40443d::TemplatedInheriting_T9227782344077896555);
  CURRENT_NAMESPACE_TYPE(TemplatedInheriting_T9200000002835747520, current_userspace_b528c6626c40443d::TemplatedInheriting_T9200000002835747520);
  CURRENT_NAMESPACE_TYPE(B2, current_userspace_b528c6626c40443d::B2);
  CURRENT_NAMESPACE_TYPE(Templated_T9227782344077896555, current_userspace_b528c6626c40443d::Templated_T9227782344077896555);
  CURRENT_NAMESPACE_TYPE(X, current_userspace_b528c6626c40443d::X);
  CURRENT_NAMESPACE_TYPE(MyFreakingVariant, current_userspace_b528c6626c40443d::MyFreakingVariant);
  CURRENT_NAMESPACE_TYPE(Variant_B_A_X_Y_E, current_userspace_b528c6626c40443d::Variant_B_A_X_Y_E);
  CURRENT_NAMESPACE_TYPE(Variant_B_A_B_B2_C_Empty_E, current_userspace_b528c6626c40443d::Variant_B_A_B_B2_C_Empty_E);
};  // CURRENT_NAMESPACE(USERSPACE_B528C6626C40443D)

namespace current {
namespace type_evolution {

// Default evolution for `CURRENT_ENUM(E)`.
#ifndef DEFAULT_EVOLUTION_575A8E9D5E0FDD2A47B4B09892E5B7B2108084F2A5F40A53CCF566F91BA93637  // USERSPACE_B528C6626C40443D::E
#define DEFAULT_EVOLUTION_575A8E9D5E0FDD2A47B4B09892E5B7B2108084F2A5F40A53CCF566F91BA93637  // USERSPACE_B528C6626C40443D::E
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, USERSPACE_B528C6626C40443D::E, EVOLUTOR> {
  template <typename INTO>
  static void Go(USERSPACE_B528C6626C40443D::E from,
                 typename INTO::E& into) {
    // TODO(dkorolev): Check enum underlying type, but not too strictly to be extensible.
    into = static_cast<typename INTO::E>(from);
  }
};
#endif

// Default evolution for struct `Empty`.
#ifndef DEFAULT_EVOLUTION_D1769B42377E1CFFF41BC86CF219FAE241A0D611CAFF3DFF38546D1E0549E289  // typename USERSPACE_B528C6626C40443D::Empty
#define DEFAULT_EVOLUTION_D1769B42377E1CFFF41BC86CF219FAE241A0D611CAFF3DFF38546D1E0549E289  // typename USERSPACE_B528C6626C40443D::Empty
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B528C6626C40443D::Empty, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B528C6626C40443D, CHECK>::value>>
  static void Go(const typename USERSPACE_B528C6626C40443D::Empty& from,
                 typename INTO::Empty& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Empty>::value == 0,
                    "Custom evolutor required.");
      static_cast<void>(from);
      static_cast<void>(into);
  }
};
#endif

// Default evolution for struct `FullTest`.
#ifndef DEFAULT_EVOLUTION_92E7E08FB52E3F8D01FE08667DDF92C3DCF6C3F7DA34AC3402C0FC73732FBE07  // typename USERSPACE_B528C6626C40443D::FullTest
#define DEFAULT_EVOLUTION_92E7E08FB52E3F8D01FE08667DDF92C3DCF6C3F7DA34AC3402C0FC73732FBE07  // typename USERSPACE_B528C6626C40443D::FullTest
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B528C6626C40443D::FullTest, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B528C6626C40443D, CHECK>::value>>
  static void Go(const typename USERSPACE_B528C6626C40443D::FullTest& from,
                 typename INTO::FullTest& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::FullTest>::value == 13,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.primitives), EVOLUTOR>::template Go<INTO>(from.primitives, into.primitives);
      Evolve<FROM, decltype(from.v1), EVOLUTOR>::template Go<INTO>(from.v1, into.v1);
      Evolve<FROM, decltype(from.v2), EVOLUTOR>::template Go<INTO>(from.v2, into.v2);
      Evolve<FROM, decltype(from.p), EVOLUTOR>::template Go<INTO>(from.p, into.p);
      Evolve<FROM, decltype(from.o), EVOLUTOR>::template Go<INTO>(from.o, into.o);
      Evolve<FROM, decltype(from.q), EVOLUTOR>::template Go<INTO>(from.q, into.q);
      Evolve<FROM, decltype(from.w1), EVOLUTOR>::template Go<INTO>(from.w1, into.w1);
      Evolve<FROM, decltype(from.w2), EVOLUTOR>::template Go<INTO>(from.w2, into.w2);
      Evolve<FROM, decltype(from.w3), EVOLUTOR>::template Go<INTO>(from.w3, into.w3);
      Evolve<FROM, decltype(from.w4), EVOLUTOR>::template Go<INTO>(from.w4, into.w4);
      Evolve<FROM, decltype(from.w5), EVOLUTOR>::template Go<INTO>(from.w5, into.w5);
      Evolve<FROM, decltype(from.w6), EVOLUTOR>::template Go<INTO>(from.w6, into.w6);
      Evolve<FROM, decltype(from.tsc), EVOLUTOR>::template Go<INTO>(from.tsc, into.tsc);
  }
};
#endif

// Default evolution for struct `B`.
#ifndef DEFAULT_EVOLUTION_A0190186DAE927C18CB6B7CF3CB98D6912C70C7D9B2F8F677E0E412E5C9FB5AF  // typename USERSPACE_B528C6626C40443D::B
#define DEFAULT_EVOLUTION_A0190186DAE927C18CB6B7CF3CB98D6912C70C7D9B2F8F677E0E412E5C9FB5AF  // typename USERSPACE_B528C6626C40443D::B
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B528C6626C40443D::B, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B528C6626C40443D, CHECK>::value>>
  static void Go(const typename USERSPACE_B528C6626C40443D::B& from,
                 typename INTO::B& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::B>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, USERSPACE_B528C6626C40443D::A, EVOLUTOR>::template Go<INTO>(static_cast<const typename FROM::A&>(from), static_cast<typename INTO::A&>(into));
      Evolve<FROM, decltype(from.b), EVOLUTOR>::template Go<INTO>(from.b, into.b);
  }
};
#endif

// Default evolution for struct `Templated_T9209626390174323094`.
#ifndef DEFAULT_EVOLUTION_D75D20D2354B6B9E9E20A942C3EE9D56058AC2A3D85C361C1D1B7D3494E280BC  // typename USERSPACE_B528C6626C40443D::Templated_T9209626390174323094
#define DEFAULT_EVOLUTION_D75D20D2354B6B9E9E20A942C3EE9D56058AC2A3D85C361C1D1B7D3494E280BC  // typename USERSPACE_B528C6626C40443D::Templated_T9209626390174323094
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B528C6626C40443D::Templated_T9209626390174323094, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B528C6626C40443D, CHECK>::value>>
  static void Go(const typename USERSPACE_B528C6626C40443D::Templated_T9209626390174323094& from,
                 typename INTO::Templated_T9209626390174323094& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Templated_T9209626390174323094>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.foo), EVOLUTOR>::template Go<INTO>(from.foo, into.foo);
      Evolve<FROM, decltype(from.bar), EVOLUTOR>::template Go<INTO>(from.bar, into.bar);
  }
};
#endif

// Default evolution for struct `Templated_T9200000002835747520`.
#ifndef DEFAULT_EVOLUTION_6CBA688E2C8BA37BAE4E748514BC412F09FCF45CDBB988646166052301DCAB9D  // typename USERSPACE_B528C6626C40443D::Templated_T9200000002835747520
#define DEFAULT_EVOLUTION_6CBA688E2C8BA37BAE4E748514BC412F09FCF45CDBB988646166052301DCAB9D  // typename USERSPACE_B528C6626C40443D::Templated_T9200000002835747520
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B528C6626C40443D::Templated_T9200000002835747520, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B528C6626C40443D, CHECK>::value>>
  static void Go(const typename USERSPACE_B528C6626C40443D::Templated_T9200000002835747520& from,
                 typename INTO::Templated_T9200000002835747520& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Templated_T9200000002835747520>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.foo), EVOLUTOR>::template Go<INTO>(from.foo, into.foo);
      Evolve<FROM, decltype(from.bar), EVOLUTOR>::template Go<INTO>(from.bar, into.bar);
  }
};
#endif

// Default evolution for struct `C`.
#ifndef DEFAULT_EVOLUTION_3CCB89F02ACD271F5E219EE21D2FB662F46DB5070217725C06518EA893BAFFF3  // typename USERSPACE_B528C6626C40443D::C
#define DEFAULT_EVOLUTION_3CCB89F02ACD271F5E219EE21D2FB662F46DB5070217725C06518EA893BAFFF3  // typename USERSPACE_B528C6626C40443D::C
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B528C6626C40443D::C, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B528C6626C40443D, CHECK>::value>>
  static void Go(const typename USERSPACE_B528C6626C40443D::C& from,
                 typename INTO::C& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::C>::value == 3,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.e), EVOLUTOR>::template Go<INTO>(from.e, into.e);
      Evolve<FROM, decltype(from.c), EVOLUTOR>::template Go<INTO>(from.c, into.c);
      Evolve<FROM, decltype(from.d), EVOLUTOR>::template Go<INTO>(from.d, into.d);
  }
};
#endif

// Default evolution for struct `TrickyEvolutionCases`.
#ifndef DEFAULT_EVOLUTION_8BB0910C2C5E539AE4ED3DBA68BB13F6BF0C1DEE515A83C609A707056F2FA049  // typename USERSPACE_B528C6626C40443D::TrickyEvolutionCases
#define DEFAULT_EVOLUTION_8BB0910C2C5E539AE4ED3DBA68BB13F6BF0C1DEE515A83C609A707056F2FA049  // typename USERSPACE_B528C6626C40443D::TrickyEvolutionCases
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B528C6626C40443D::TrickyEvolutionCases, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B528C6626C40443D, CHECK>::value>>
  static void Go(const typename USERSPACE_B528C6626C40443D::TrickyEvolutionCases& from,
                 typename INTO::TrickyEvolutionCases& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TrickyEvolutionCases>::value == 7,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.o1), EVOLUTOR>::template Go<INTO>(from.o1, into.o1);
      Evolve<FROM, decltype(from.o2), EVOLUTOR>::template Go<INTO>(from.o2, into.o2);
      Evolve<FROM, decltype(from.o3), EVOLUTOR>::template Go<INTO>(from.o3, into.o3);
      Evolve<FROM, decltype(from.o4), EVOLUTOR>::template Go<INTO>(from.o4, into.o4);
      Evolve<FROM, decltype(from.o5), EVOLUTOR>::template Go<INTO>(from.o5, into.o5);
      Evolve<FROM, decltype(from.o6), EVOLUTOR>::template Go<INTO>(from.o6, into.o6);
      Evolve<FROM, decltype(from.o7), EVOLUTOR>::template Go<INTO>(from.o7, into.o7);
  }
};
#endif

// Default evolution for struct `TemplatedInheriting_T9201673071807149456`.
#ifndef DEFAULT_EVOLUTION_C24F0CDAA72AAEDB738E21266F4502BCFF1DBA9964C378CF6012D8CB350E0ED8  // typename USERSPACE_B528C6626C40443D::TemplatedInheriting_T9201673071807149456
#define DEFAULT_EVOLUTION_C24F0CDAA72AAEDB738E21266F4502BCFF1DBA9964C378CF6012D8CB350E0ED8  // typename USERSPACE_B528C6626C40443D::TemplatedInheriting_T9201673071807149456
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B528C6626C40443D::TemplatedInheriting_T9201673071807149456, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B528C6626C40443D, CHECK>::value>>
  static void Go(const typename USERSPACE_B528C6626C40443D::TemplatedInheriting_T9201673071807149456& from,
                 typename INTO::TemplatedInheriting_T9201673071807149456& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TemplatedInheriting_T9201673071807149456>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, USERSPACE_B528C6626C40443D::A, EVOLUTOR>::template Go<INTO>(static_cast<const typename FROM::A&>(from), static_cast<typename INTO::A&>(into));
      Evolve<FROM, decltype(from.baz), EVOLUTOR>::template Go<INTO>(from.baz, into.baz);
      Evolve<FROM, decltype(from.meh), EVOLUTOR>::template Go<INTO>(from.meh, into.meh);
  }
};
#endif

// Default evolution for struct `A`.
#ifndef DEFAULT_EVOLUTION_188B64059127FE077D880D0BAFDEC49EA1F025953035BDC7D5E2937FD79110F8  // typename USERSPACE_B528C6626C40443D::A
#define DEFAULT_EVOLUTION_188B64059127FE077D880D0BAFDEC49EA1F025953035BDC7D5E2937FD79110F8  // typename USERSPACE_B528C6626C40443D::A
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B528C6626C40443D::A, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B528C6626C40443D, CHECK>::value>>
  static void Go(const typename USERSPACE_B528C6626C40443D::A& from,
                 typename INTO::A& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::A>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.a), EVOLUTOR>::template Go<INTO>(from.a, into.a);
  }
};
#endif

// Default evolution for struct `Primitives`.
#ifndef DEFAULT_EVOLUTION_8DE314CA03F945A5974306EFFB1968E691B1F19421859FE89E09FCE490945C55  // typename USERSPACE_B528C6626C40443D::Primitives
#define DEFAULT_EVOLUTION_8DE314CA03F945A5974306EFFB1968E691B1F19421859FE89E09FCE490945C55  // typename USERSPACE_B528C6626C40443D::Primitives
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B528C6626C40443D::Primitives, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B528C6626C40443D, CHECK>::value>>
  static void Go(const typename USERSPACE_B528C6626C40443D::Primitives& from,
                 typename INTO::Primitives& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Primitives>::value == 15,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.a), EVOLUTOR>::template Go<INTO>(from.a, into.a);
      Evolve<FROM, decltype(from.b), EVOLUTOR>::template Go<INTO>(from.b, into.b);
      Evolve<FROM, decltype(from.c), EVOLUTOR>::template Go<INTO>(from.c, into.c);
      Evolve<FROM, decltype(from.d), EVOLUTOR>::template Go<INTO>(from.d, into.d);
      Evolve<FROM, decltype(from.e), EVOLUTOR>::template Go<INTO>(from.e, into.e);
      Evolve<FROM, decltype(from.f), EVOLUTOR>::template Go<INTO>(from.f, into.f);
      Evolve<FROM, decltype(from.g), EVOLUTOR>::template Go<INTO>(from.g, into.g);
      Evolve<FROM, decltype(from.h), EVOLUTOR>::template Go<INTO>(from.h, into.h);
      Evolve<FROM, decltype(from.i), EVOLUTOR>::template Go<INTO>(from.i, into.i);
      Evolve<FROM, decltype(from.j), EVOLUTOR>::template Go<INTO>(from.j, into.j);
      Evolve<FROM, decltype(from.k), EVOLUTOR>::template Go<INTO>(from.k, into.k);
      Evolve<FROM, decltype(from.l), EVOLUTOR>::template Go<INTO>(from.l, into.l);
      Evolve<FROM, decltype(from.m), EVOLUTOR>::template Go<INTO>(from.m, into.m);
      Evolve<FROM, decltype(from.n), EVOLUTOR>::template Go<INTO>(from.n, into.n);
      Evolve<FROM, decltype(from.o), EVOLUTOR>::template Go<INTO>(from.o, into.o);
  }
};
#endif

// Default evolution for struct `TemplatedInheriting_T9209980946934124423`.
#ifndef DEFAULT_EVOLUTION_E7C9147453BE2A0ADFBF92D57F0A033E848C754D15E7106015A61FBCE4DA723A  // typename USERSPACE_B528C6626C40443D::TemplatedInheriting_T9209980946934124423
#define DEFAULT_EVOLUTION_E7C9147453BE2A0ADFBF92D57F0A033E848C754D15E7106015A61FBCE4DA723A  // typename USERSPACE_B528C6626C40443D::TemplatedInheriting_T9209980946934124423
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B528C6626C40443D::TemplatedInheriting_T9209980946934124423, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B528C6626C40443D, CHECK>::value>>
  static void Go(const typename USERSPACE_B528C6626C40443D::TemplatedInheriting_T9209980946934124423& from,
                 typename INTO::TemplatedInheriting_T9209980946934124423& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TemplatedInheriting_T9209980946934124423>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, USERSPACE_B528C6626C40443D::A, EVOLUTOR>::template Go<INTO>(static_cast<const typename FROM::A&>(from), static_cast<typename INTO::A&>(into));
      Evolve<FROM, decltype(from.baz), EVOLUTOR>::template Go<INTO>(from.baz, into.baz);
      Evolve<FROM, decltype(from.meh), EVOLUTOR>::template Go<INTO>(from.meh, into.meh);
  }
};
#endif

// Default evolution for struct `Y`.
#ifndef DEFAULT_EVOLUTION_542B5A18BD21D7F57F29A2B8DEC58E984A79D3809270590B3079FBCFA5C4249B  // typename USERSPACE_B528C6626C40443D::Y
#define DEFAULT_EVOLUTION_542B5A18BD21D7F57F29A2B8DEC58E984A79D3809270590B3079FBCFA5C4249B  // typename USERSPACE_B528C6626C40443D::Y
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B528C6626C40443D::Y, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B528C6626C40443D, CHECK>::value>>
  static void Go(const typename USERSPACE_B528C6626C40443D::Y& from,
                 typename INTO::Y& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Y>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.e), EVOLUTOR>::template Go<INTO>(from.e, into.e);
  }
};
#endif

// Default evolution for struct `Templated_T9209980946934124423`.
#ifndef DEFAULT_EVOLUTION_93E57D8978E6D8F37D5D5EB1D768781930CC9CF4EB5C3F9806311B549F0E9777  // typename USERSPACE_B528C6626C40443D::Templated_T9209980946934124423
#define DEFAULT_EVOLUTION_93E57D8978E6D8F37D5D5EB1D768781930CC9CF4EB5C3F9806311B549F0E9777  // typename USERSPACE_B528C6626C40443D::Templated_T9209980946934124423
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B528C6626C40443D::Templated_T9209980946934124423, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B528C6626C40443D, CHECK>::value>>
  static void Go(const typename USERSPACE_B528C6626C40443D::Templated_T9209980946934124423& from,
                 typename INTO::Templated_T9209980946934124423& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Templated_T9209980946934124423>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.foo), EVOLUTOR>::template Go<INTO>(from.foo, into.foo);
      Evolve<FROM, decltype(from.bar), EVOLUTOR>::template Go<INTO>(from.bar, into.bar);
  }
};
#endif

// Default evolution for struct `TemplatedInheriting_T9227782344077896555`.
#ifndef DEFAULT_EVOLUTION_9099DF484A47B6495B26A5D1C83856267DD82EB3EF105479355FD153F8A24A81  // typename USERSPACE_B528C6626C40443D::TemplatedInheriting_T9227782344077896555
#define DEFAULT_EVOLUTION_9099DF484A47B6495B26A5D1C83856267DD82EB3EF105479355FD153F8A24A81  // typename USERSPACE_B528C6626C40443D::TemplatedInheriting_T9227782344077896555
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B528C6626C40443D::TemplatedInheriting_T9227782344077896555, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B528C6626C40443D, CHECK>::value>>
  static void Go(const typename USERSPACE_B528C6626C40443D::TemplatedInheriting_T9227782344077896555& from,
                 typename INTO::TemplatedInheriting_T9227782344077896555& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TemplatedInheriting_T9227782344077896555>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, USERSPACE_B528C6626C40443D::A, EVOLUTOR>::template Go<INTO>(static_cast<const typename FROM::A&>(from), static_cast<typename INTO::A&>(into));
      Evolve<FROM, decltype(from.baz), EVOLUTOR>::template Go<INTO>(from.baz, into.baz);
      Evolve<FROM, decltype(from.meh), EVOLUTOR>::template Go<INTO>(from.meh, into.meh);
  }
};
#endif

// Default evolution for struct `TemplatedInheriting_T9200000002835747520`.
#ifndef DEFAULT_EVOLUTION_8A160338E745F72CD9F0E5D72544E2445975B30C29CAB1720BCE5434D13178C8  // typename USERSPACE_B528C6626C40443D::TemplatedInheriting_T9200000002835747520
#define DEFAULT_EVOLUTION_8A160338E745F72CD9F0E5D72544E2445975B30C29CAB1720BCE5434D13178C8  // typename USERSPACE_B528C6626C40443D::TemplatedInheriting_T9200000002835747520
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B528C6626C40443D::TemplatedInheriting_T9200000002835747520, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B528C6626C40443D, CHECK>::value>>
  static void Go(const typename USERSPACE_B528C6626C40443D::TemplatedInheriting_T9200000002835747520& from,
                 typename INTO::TemplatedInheriting_T9200000002835747520& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TemplatedInheriting_T9200000002835747520>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, USERSPACE_B528C6626C40443D::A, EVOLUTOR>::template Go<INTO>(static_cast<const typename FROM::A&>(from), static_cast<typename INTO::A&>(into));
      Evolve<FROM, decltype(from.baz), EVOLUTOR>::template Go<INTO>(from.baz, into.baz);
      Evolve<FROM, decltype(from.meh), EVOLUTOR>::template Go<INTO>(from.meh, into.meh);
  }
};
#endif

// Default evolution for struct `B2`.
#ifndef DEFAULT_EVOLUTION_3DC2460BF120F3B58DF16B35067A34ABD2B0BC15B1F88D614FF91B8A6DB2F175  // typename USERSPACE_B528C6626C40443D::B2
#define DEFAULT_EVOLUTION_3DC2460BF120F3B58DF16B35067A34ABD2B0BC15B1F88D614FF91B8A6DB2F175  // typename USERSPACE_B528C6626C40443D::B2
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B528C6626C40443D::B2, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B528C6626C40443D, CHECK>::value>>
  static void Go(const typename USERSPACE_B528C6626C40443D::B2& from,
                 typename INTO::B2& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::B2>::value == 0,
                    "Custom evolutor required.");
      Evolve<FROM, USERSPACE_B528C6626C40443D::A, EVOLUTOR>::template Go<INTO>(static_cast<const typename FROM::A&>(from), static_cast<typename INTO::A&>(into));
      static_cast<void>(from);
      static_cast<void>(into);
  }
};
#endif

// Default evolution for struct `Templated_T9227782344077896555`.
#ifndef DEFAULT_EVOLUTION_56DF4808A0580710698DB83100684C120FAD035F2B2C15E37269A95560267880  // typename USERSPACE_B528C6626C40443D::Templated_T9227782344077896555
#define DEFAULT_EVOLUTION_56DF4808A0580710698DB83100684C120FAD035F2B2C15E37269A95560267880  // typename USERSPACE_B528C6626C40443D::Templated_T9227782344077896555
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B528C6626C40443D::Templated_T9227782344077896555, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B528C6626C40443D, CHECK>::value>>
  static void Go(const typename USERSPACE_B528C6626C40443D::Templated_T9227782344077896555& from,
                 typename INTO::Templated_T9227782344077896555& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Templated_T9227782344077896555>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.foo), EVOLUTOR>::template Go<INTO>(from.foo, into.foo);
      Evolve<FROM, decltype(from.bar), EVOLUTOR>::template Go<INTO>(from.bar, into.bar);
  }
};
#endif

// Default evolution for struct `X`.
#ifndef DEFAULT_EVOLUTION_B449D5D7BE7CB2F9C72DA0DE0ACDB0764F1BB17BD1B67D411F1CEB14EA441F92  // typename USERSPACE_B528C6626C40443D::X
#define DEFAULT_EVOLUTION_B449D5D7BE7CB2F9C72DA0DE0ACDB0764F1BB17BD1B67D411F1CEB14EA441F92  // typename USERSPACE_B528C6626C40443D::X
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_B528C6626C40443D::X, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B528C6626C40443D, CHECK>::value>>
  static void Go(const typename USERSPACE_B528C6626C40443D::X& from,
                 typename INTO::X& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::X>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.x), EVOLUTOR>::template Go<INTO>(from.x, into.x);
  }
};
#endif

// Default evolution for `Optional<A>`.
#ifndef DEFAULT_EVOLUTION_9F333E37E689CCE2128DC652AFAECA4F93EF1D1C7EB432A51CA91B66D996EFE7  // Optional<typename USERSPACE_B528C6626C40443D::A>
#define DEFAULT_EVOLUTION_9F333E37E689CCE2128DC652AFAECA4F93EF1D1C7EB432A51CA91B66D996EFE7  // Optional<typename USERSPACE_B528C6626C40443D::A>
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, Optional<typename USERSPACE_B528C6626C40443D::A>, EVOLUTOR> {
  template <typename INTO, typename INTO_TYPE>
  static void Go(const Optional<typename USERSPACE_B528C6626C40443D::A>& from, INTO_TYPE& into) {
    if (Exists(from)) {
      typename INTO::A evolved;
      Evolve<FROM, typename USERSPACE_B528C6626C40443D::A, EVOLUTOR>::template Go<INTO>(Value(from), evolved);
      into = evolved;
    } else {
      into = nullptr;
    }
  }
};
#endif

// Default evolution for `Optional<Primitives>`.
#ifndef DEFAULT_EVOLUTION_9FF16E8FAC0989FC12D7634B68D824D759CD27F1121686B87C72C7AF215A0193  // Optional<typename USERSPACE_B528C6626C40443D::Primitives>
#define DEFAULT_EVOLUTION_9FF16E8FAC0989FC12D7634B68D824D759CD27F1121686B87C72C7AF215A0193  // Optional<typename USERSPACE_B528C6626C40443D::Primitives>
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, Optional<typename USERSPACE_B528C6626C40443D::Primitives>, EVOLUTOR> {
  template <typename INTO, typename INTO_TYPE>
  static void Go(const Optional<typename USERSPACE_B528C6626C40443D::Primitives>& from, INTO_TYPE& into) {
    if (Exists(from)) {
      typename INTO::Primitives evolved;
      Evolve<FROM, typename USERSPACE_B528C6626C40443D::Primitives, EVOLUTOR>::template Go<INTO>(Value(from), evolved);
      into = evolved;
    } else {
      into = nullptr;
    }
  }
};
#endif

// Default evolution for `Optional<std::vector<A>>`.
#ifndef DEFAULT_EVOLUTION_713009CA526E5FF51E381D59A7B131C6A2F943AE28CE50EC422421E7E523909B  // Optional<std::vector<typename USERSPACE_B528C6626C40443D::A>>
#define DEFAULT_EVOLUTION_713009CA526E5FF51E381D59A7B131C6A2F943AE28CE50EC422421E7E523909B  // Optional<std::vector<typename USERSPACE_B528C6626C40443D::A>>
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, Optional<std::vector<typename USERSPACE_B528C6626C40443D::A>>, EVOLUTOR> {
  template <typename INTO, typename INTO_TYPE>
  static void Go(const Optional<std::vector<typename USERSPACE_B528C6626C40443D::A>>& from, INTO_TYPE& into) {
    if (Exists(from)) {
      std::vector<typename INTO::A> evolved;
      Evolve<FROM, std::vector<typename USERSPACE_B528C6626C40443D::A>, EVOLUTOR>::template Go<INTO>(Value(from), evolved);
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
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, Optional<std::vector<int32_t>>, EVOLUTOR> {
  template <typename INTO, typename INTO_TYPE>
  static void Go(const Optional<std::vector<int32_t>>& from, INTO_TYPE& into) {
    if (Exists(from)) {
      std::vector<int32_t> evolved;
      Evolve<FROM, std::vector<int32_t>, EVOLUTOR>::template Go<INTO>(Value(from), evolved);
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
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, Optional<std::vector<std::string>>, EVOLUTOR> {
  template <typename INTO, typename INTO_TYPE>
  static void Go(const Optional<std::vector<std::string>>& from, INTO_TYPE& into) {
    if (Exists(from)) {
      std::vector<std::string> evolved;
      Evolve<FROM, std::vector<std::string>, EVOLUTOR>::template Go<INTO>(Value(from), evolved);
      into = evolved;
    } else {
      into = nullptr;
    }
  }
};
#endif

// Default evolution for `Variant<A, X, Y>`.
#ifndef DEFAULT_EVOLUTION_87EDBB3FF5B41488EC3944456E1FA523D78BF2C7D4A6D173CB0068C30590E2EA  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B528C6626C40443D::A, USERSPACE_B528C6626C40443D::X, USERSPACE_B528C6626C40443D::Y>>
#define DEFAULT_EVOLUTION_87EDBB3FF5B41488EC3944456E1FA523D78BF2C7D4A6D173CB0068C30590E2EA  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B528C6626C40443D::A, USERSPACE_B528C6626C40443D::X, USERSPACE_B528C6626C40443D::Y>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_B528C6626C40443D_MyFreakingVariant_Cases {
  DST& into;
  explicit USERSPACE_B528C6626C40443D_MyFreakingVariant_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::A& value) const {
    using into_t = typename INTO::A;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::A, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::X& value) const {
    using into_t = typename INTO::X;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::X, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::Y& value) const {
    using into_t = typename INTO::Y;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::Y, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename FROM, typename EVOLUTOR, typename VARIANT_NAME_HELPER>
struct Evolve<FROM, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B528C6626C40443D::A, USERSPACE_B528C6626C40443D::X, USERSPACE_B528C6626C40443D::Y>>, EVOLUTOR> {
  // TODO(dkorolev): A `static_assert` to ensure the number of cases is the same.
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B528C6626C40443D, CHECK>::value>>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B528C6626C40443D::A, USERSPACE_B528C6626C40443D::X, USERSPACE_B528C6626C40443D::Y>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(USERSPACE_B528C6626C40443D_MyFreakingVariant_Cases<decltype(into), FROM, INTO, EVOLUTOR>(into));
  }
};
#endif

// Default evolution for `Variant<A, B, B2, C, Empty>`.
#ifndef DEFAULT_EVOLUTION_FD6D1EDCA537BCDF2BCE9AEB4B7F2BDAC1BCEC2449EA667D6A3009DBECADCF79  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B528C6626C40443D::A, USERSPACE_B528C6626C40443D::B, USERSPACE_B528C6626C40443D::B2, USERSPACE_B528C6626C40443D::C, USERSPACE_B528C6626C40443D::Empty>>
#define DEFAULT_EVOLUTION_FD6D1EDCA537BCDF2BCE9AEB4B7F2BDAC1BCEC2449EA667D6A3009DBECADCF79  // ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B528C6626C40443D::A, USERSPACE_B528C6626C40443D::B, USERSPACE_B528C6626C40443D::B2, USERSPACE_B528C6626C40443D::C, USERSPACE_B528C6626C40443D::Empty>>
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_B528C6626C40443D_Variant_B_A_B_B2_C_Empty_E_Cases {
  DST& into;
  explicit USERSPACE_B528C6626C40443D_Variant_B_A_B_B2_C_Empty_E_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::A& value) const {
    using into_t = typename INTO::A;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::A, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::B& value) const {
    using into_t = typename INTO::B;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::B, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::B2& value) const {
    using into_t = typename INTO::B2;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::B2, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::C& value) const {
    using into_t = typename INTO::C;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::C, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::Empty& value) const {
    using into_t = typename INTO::Empty;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::Empty, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename FROM, typename EVOLUTOR, typename VARIANT_NAME_HELPER>
struct Evolve<FROM, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B528C6626C40443D::A, USERSPACE_B528C6626C40443D::B, USERSPACE_B528C6626C40443D::B2, USERSPACE_B528C6626C40443D::C, USERSPACE_B528C6626C40443D::Empty>>, EVOLUTOR> {
  // TODO(dkorolev): A `static_assert` to ensure the number of cases is the same.
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_B528C6626C40443D, CHECK>::value>>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_B528C6626C40443D::A, USERSPACE_B528C6626C40443D::B, USERSPACE_B528C6626C40443D::B2, USERSPACE_B528C6626C40443D::C, USERSPACE_B528C6626C40443D::Empty>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(USERSPACE_B528C6626C40443D_Variant_B_A_B_B2_C_Empty_E_Cases<decltype(into), FROM, INTO, EVOLUTOR>(into));
  }
};
#endif

}  // namespace current::type_evolution
}  // namespace current

// Privileged types.
CURRENT_DERIVED_NAMESPACE(ExposedNamespace, USERSPACE_B528C6626C40443D) {
  CURRENT_NAMESPACE_TYPE(Empty, current_userspace_b528c6626c40443d::Empty);
  CURRENT_NAMESPACE_TYPE(ExposedFullTest, current_userspace_b528c6626c40443d::FullTest);
  CURRENT_NAMESPACE_TYPE(Primitives, current_userspace_b528c6626c40443d::Primitives);
};  // CURRENT_NAMESPACE(ExposedNamespace)

// clang-format on

#endif  // CURRENT_USERSPACE_B528C6626C40443D
