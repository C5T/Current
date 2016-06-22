// The `current.h` file is the one from `https://github.com/C5T/Current`.
// Compile with `-std=c++11` or higher.

#ifndef CURRENT_USERSPACE_15964E7953DBD27D
#define CURRENT_USERSPACE_15964E7953DBD27D

#include "current.h"

// clang-format off

namespace current_userspace_15964e7953dbd27d {

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

CURRENT_STRUCT(TrickySchemaCases) {
  CURRENT_FIELD(o1, Optional<std::string>);
  CURRENT_FIELD(o2, Optional<int32_t>);
  CURRENT_FIELD(o3, Optional<std::vector<std::string>>);
  CURRENT_FIELD(o4, Optional<std::vector<int32_t>>);
  CURRENT_FIELD(o5, Optional<std::vector<A>>);
  CURRENT_FIELD(o6, (std::pair<std::string, Optional<A>>));
  CURRENT_FIELD(o7, (std::map<std::string, Optional<A>>));
};
using T9204352955776062011 = TrickySchemaCases;

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
  CURRENT_FIELD(tsc, TrickySchemaCases);
};
using T9208845362562817309 = FullTest;

}  // namespace current_userspace_15964e7953dbd27d

CURRENT_NAMESPACE(USERSPACE_15964E7953DBD27D) {
  CURRENT_NAMESPACE_TYPE(E, current_userspace_15964e7953dbd27d::E);
  CURRENT_NAMESPACE_TYPE(Empty, current_userspace_15964e7953dbd27d::Empty);
  CURRENT_NAMESPACE_TYPE(B, current_userspace_15964e7953dbd27d::B);
  CURRENT_NAMESPACE_TYPE(Templated_T9209626390174323094, current_userspace_15964e7953dbd27d::Templated_T9209626390174323094);
  CURRENT_NAMESPACE_TYPE(Templated_T9200000002835747520, current_userspace_15964e7953dbd27d::Templated_T9200000002835747520);
  CURRENT_NAMESPACE_TYPE(C, current_userspace_15964e7953dbd27d::C);
  CURRENT_NAMESPACE_TYPE(TrickySchemaCases, current_userspace_15964e7953dbd27d::TrickySchemaCases);
  CURRENT_NAMESPACE_TYPE(TemplatedInheriting_T9201673071807149456, current_userspace_15964e7953dbd27d::TemplatedInheriting_T9201673071807149456);
  CURRENT_NAMESPACE_TYPE(A, current_userspace_15964e7953dbd27d::A);
  CURRENT_NAMESPACE_TYPE(Primitives, current_userspace_15964e7953dbd27d::Primitives);
  CURRENT_NAMESPACE_TYPE(TemplatedInheriting_T9209980946934124423, current_userspace_15964e7953dbd27d::TemplatedInheriting_T9209980946934124423);
  CURRENT_NAMESPACE_TYPE(Y, current_userspace_15964e7953dbd27d::Y);
  CURRENT_NAMESPACE_TYPE(FullTest, current_userspace_15964e7953dbd27d::FullTest);
  CURRENT_NAMESPACE_TYPE(Templated_T9209980946934124423, current_userspace_15964e7953dbd27d::Templated_T9209980946934124423);
  CURRENT_NAMESPACE_TYPE(TemplatedInheriting_T9227782344077896555, current_userspace_15964e7953dbd27d::TemplatedInheriting_T9227782344077896555);
  CURRENT_NAMESPACE_TYPE(TemplatedInheriting_T9200000002835747520, current_userspace_15964e7953dbd27d::TemplatedInheriting_T9200000002835747520);
  CURRENT_NAMESPACE_TYPE(B2, current_userspace_15964e7953dbd27d::B2);
  CURRENT_NAMESPACE_TYPE(Templated_T9227782344077896555, current_userspace_15964e7953dbd27d::Templated_T9227782344077896555);
  CURRENT_NAMESPACE_TYPE(X, current_userspace_15964e7953dbd27d::X);
  CURRENT_NAMESPACE_TYPE(MyFreakingVariant, current_userspace_15964e7953dbd27d::MyFreakingVariant);
  CURRENT_NAMESPACE_TYPE(Variant_B_A_X_Y_E, current_userspace_15964e7953dbd27d::Variant_B_A_X_Y_E);
  CURRENT_NAMESPACE_TYPE(Variant_B_A_B_B2_C_Empty_E, current_userspace_15964e7953dbd27d::Variant_B_A_B_B2_C_Empty_E);
};  // CURRENT_NAMESPACE(USERSPACE_15964E7953DBD27D)

namespace current {
namespace type_evolution {

// Default evolution for `CURRENT_ENUM(E)`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, USERSPACE_15964E7953DBD27D::E, EVOLUTOR> {
  template <typename INTO>
  static void Go(USERSPACE_15964E7953DBD27D::E from,
                 typename INTO::E& into) {
    // TODO(dkorolev): Check enum underlying type, but not too strictly to be extensible.
    into = static_cast<typename INTO::E>(from);
  }
};

// Default evolution for struct `Empty`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_15964E7953DBD27D::Empty, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_15964E7953DBD27D, CHECK>::value>>
  static void Go(const typename USERSPACE_15964E7953DBD27D::Empty& from,
                 typename INTO::Empty& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Empty>::value == 0,
                    "Custom evolutor required.");
      static_cast<void>(from);
      static_cast<void>(into);
  }
};

// Default evolution for struct `B`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_15964E7953DBD27D::B, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_15964E7953DBD27D, CHECK>::value>>
  static void Go(const typename USERSPACE_15964E7953DBD27D::B& from,
                 typename INTO::B& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::B>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, USERSPACE_15964E7953DBD27D::A, EVOLUTOR>::template Go<INTO>(static_cast<const typename FROM::A&>(from), static_cast<typename INTO::A&>(into));
      Evolve<FROM, decltype(from.b), EVOLUTOR>::template Go<INTO>(from.b, into.b);
  }
};

// Default evolution for struct `Templated_T9209626390174323094`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_15964E7953DBD27D::Templated_T9209626390174323094, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_15964E7953DBD27D, CHECK>::value>>
  static void Go(const typename USERSPACE_15964E7953DBD27D::Templated_T9209626390174323094& from,
                 typename INTO::Templated_T9209626390174323094& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Templated_T9209626390174323094>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.foo), EVOLUTOR>::template Go<INTO>(from.foo, into.foo);
      Evolve<FROM, decltype(from.bar), EVOLUTOR>::template Go<INTO>(from.bar, into.bar);
  }
};

// Default evolution for struct `Templated_T9200000002835747520`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_15964E7953DBD27D::Templated_T9200000002835747520, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_15964E7953DBD27D, CHECK>::value>>
  static void Go(const typename USERSPACE_15964E7953DBD27D::Templated_T9200000002835747520& from,
                 typename INTO::Templated_T9200000002835747520& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Templated_T9200000002835747520>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.foo), EVOLUTOR>::template Go<INTO>(from.foo, into.foo);
      Evolve<FROM, decltype(from.bar), EVOLUTOR>::template Go<INTO>(from.bar, into.bar);
  }
};

// Default evolution for struct `C`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_15964E7953DBD27D::C, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_15964E7953DBD27D, CHECK>::value>>
  static void Go(const typename USERSPACE_15964E7953DBD27D::C& from,
                 typename INTO::C& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::C>::value == 3,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.e), EVOLUTOR>::template Go<INTO>(from.e, into.e);
      Evolve<FROM, decltype(from.c), EVOLUTOR>::template Go<INTO>(from.c, into.c);
      Evolve<FROM, decltype(from.d), EVOLUTOR>::template Go<INTO>(from.d, into.d);
  }
};

// Default evolution for struct `TrickySchemaCases`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_15964E7953DBD27D::TrickySchemaCases, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_15964E7953DBD27D, CHECK>::value>>
  static void Go(const typename USERSPACE_15964E7953DBD27D::TrickySchemaCases& from,
                 typename INTO::TrickySchemaCases& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TrickySchemaCases>::value == 7,
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

// Default evolution for struct `TemplatedInheriting_T9201673071807149456`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_15964E7953DBD27D::TemplatedInheriting_T9201673071807149456, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_15964E7953DBD27D, CHECK>::value>>
  static void Go(const typename USERSPACE_15964E7953DBD27D::TemplatedInheriting_T9201673071807149456& from,
                 typename INTO::TemplatedInheriting_T9201673071807149456& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TemplatedInheriting_T9201673071807149456>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, USERSPACE_15964E7953DBD27D::A, EVOLUTOR>::template Go<INTO>(static_cast<const typename FROM::A&>(from), static_cast<typename INTO::A&>(into));
      Evolve<FROM, decltype(from.baz), EVOLUTOR>::template Go<INTO>(from.baz, into.baz);
      Evolve<FROM, decltype(from.meh), EVOLUTOR>::template Go<INTO>(from.meh, into.meh);
  }
};

// Default evolution for struct `A`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_15964E7953DBD27D::A, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_15964E7953DBD27D, CHECK>::value>>
  static void Go(const typename USERSPACE_15964E7953DBD27D::A& from,
                 typename INTO::A& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::A>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.a), EVOLUTOR>::template Go<INTO>(from.a, into.a);
  }
};

// Default evolution for struct `Primitives`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_15964E7953DBD27D::Primitives, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_15964E7953DBD27D, CHECK>::value>>
  static void Go(const typename USERSPACE_15964E7953DBD27D::Primitives& from,
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

// Default evolution for struct `TemplatedInheriting_T9209980946934124423`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_15964E7953DBD27D::TemplatedInheriting_T9209980946934124423, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_15964E7953DBD27D, CHECK>::value>>
  static void Go(const typename USERSPACE_15964E7953DBD27D::TemplatedInheriting_T9209980946934124423& from,
                 typename INTO::TemplatedInheriting_T9209980946934124423& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TemplatedInheriting_T9209980946934124423>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, USERSPACE_15964E7953DBD27D::A, EVOLUTOR>::template Go<INTO>(static_cast<const typename FROM::A&>(from), static_cast<typename INTO::A&>(into));
      Evolve<FROM, decltype(from.baz), EVOLUTOR>::template Go<INTO>(from.baz, into.baz);
      Evolve<FROM, decltype(from.meh), EVOLUTOR>::template Go<INTO>(from.meh, into.meh);
  }
};

// Default evolution for struct `Y`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_15964E7953DBD27D::Y, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_15964E7953DBD27D, CHECK>::value>>
  static void Go(const typename USERSPACE_15964E7953DBD27D::Y& from,
                 typename INTO::Y& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Y>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.e), EVOLUTOR>::template Go<INTO>(from.e, into.e);
  }
};

// Default evolution for struct `FullTest`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_15964E7953DBD27D::FullTest, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_15964E7953DBD27D, CHECK>::value>>
  static void Go(const typename USERSPACE_15964E7953DBD27D::FullTest& from,
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

// Default evolution for struct `Templated_T9209980946934124423`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_15964E7953DBD27D::Templated_T9209980946934124423, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_15964E7953DBD27D, CHECK>::value>>
  static void Go(const typename USERSPACE_15964E7953DBD27D::Templated_T9209980946934124423& from,
                 typename INTO::Templated_T9209980946934124423& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Templated_T9209980946934124423>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.foo), EVOLUTOR>::template Go<INTO>(from.foo, into.foo);
      Evolve<FROM, decltype(from.bar), EVOLUTOR>::template Go<INTO>(from.bar, into.bar);
  }
};

// Default evolution for struct `TemplatedInheriting_T9227782344077896555`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_15964E7953DBD27D::TemplatedInheriting_T9227782344077896555, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_15964E7953DBD27D, CHECK>::value>>
  static void Go(const typename USERSPACE_15964E7953DBD27D::TemplatedInheriting_T9227782344077896555& from,
                 typename INTO::TemplatedInheriting_T9227782344077896555& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TemplatedInheriting_T9227782344077896555>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, USERSPACE_15964E7953DBD27D::A, EVOLUTOR>::template Go<INTO>(static_cast<const typename FROM::A&>(from), static_cast<typename INTO::A&>(into));
      Evolve<FROM, decltype(from.baz), EVOLUTOR>::template Go<INTO>(from.baz, into.baz);
      Evolve<FROM, decltype(from.meh), EVOLUTOR>::template Go<INTO>(from.meh, into.meh);
  }
};

// Default evolution for struct `TemplatedInheriting_T9200000002835747520`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_15964E7953DBD27D::TemplatedInheriting_T9200000002835747520, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_15964E7953DBD27D, CHECK>::value>>
  static void Go(const typename USERSPACE_15964E7953DBD27D::TemplatedInheriting_T9200000002835747520& from,
                 typename INTO::TemplatedInheriting_T9200000002835747520& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::TemplatedInheriting_T9200000002835747520>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, USERSPACE_15964E7953DBD27D::A, EVOLUTOR>::template Go<INTO>(static_cast<const typename FROM::A&>(from), static_cast<typename INTO::A&>(into));
      Evolve<FROM, decltype(from.baz), EVOLUTOR>::template Go<INTO>(from.baz, into.baz);
      Evolve<FROM, decltype(from.meh), EVOLUTOR>::template Go<INTO>(from.meh, into.meh);
  }
};

// Default evolution for struct `B2`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_15964E7953DBD27D::B2, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_15964E7953DBD27D, CHECK>::value>>
  static void Go(const typename USERSPACE_15964E7953DBD27D::B2& from,
                 typename INTO::B2& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::B2>::value == 0,
                    "Custom evolutor required.");
      Evolve<FROM, USERSPACE_15964E7953DBD27D::A, EVOLUTOR>::template Go<INTO>(static_cast<const typename FROM::A&>(from), static_cast<typename INTO::A&>(into));
      static_cast<void>(from);
      static_cast<void>(into);
  }
};

// Default evolution for struct `Templated_T9227782344077896555`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_15964E7953DBD27D::Templated_T9227782344077896555, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_15964E7953DBD27D, CHECK>::value>>
  static void Go(const typename USERSPACE_15964E7953DBD27D::Templated_T9227782344077896555& from,
                 typename INTO::Templated_T9227782344077896555& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Templated_T9227782344077896555>::value == 2,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.foo), EVOLUTOR>::template Go<INTO>(from.foo, into.foo);
      Evolve<FROM, decltype(from.bar), EVOLUTOR>::template Go<INTO>(from.bar, into.bar);
  }
};

// Default evolution for struct `X`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, typename USERSPACE_15964E7953DBD27D::X, EVOLUTOR> {
  template <typename INTO,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_15964E7953DBD27D, CHECK>::value>>
  static void Go(const typename USERSPACE_15964E7953DBD27D::X& from,
                 typename INTO::X& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::X>::value == 1,
                    "Custom evolutor required.");
      Evolve<FROM, decltype(from.x), EVOLUTOR>::template Go<INTO>(from.x, into.x);
  }
};

// Default evolution for `Optional<A>`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, Optional<typename USERSPACE_15964E7953DBD27D::A>, EVOLUTOR> {
  template <typename INTO, typename INTO_TYPE>
  static void Go(const Optional<typename USERSPACE_15964E7953DBD27D::A>& from, INTO_TYPE& into) {
    if (Exists(from)) {
      typename INTO::A evolved;
      Evolve<FROM, typename INTO::A, EVOLUTOR>::template Go<INTO>(Value(from), evolved);
      into = evolved;
    } else {
      into = nullptr;
    }
  }
};

// Default evolution for `Optional<Primitives>`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, Optional<typename USERSPACE_15964E7953DBD27D::Primitives>, EVOLUTOR> {
  template <typename INTO, typename INTO_TYPE>
  static void Go(const Optional<typename USERSPACE_15964E7953DBD27D::Primitives>& from, INTO_TYPE& into) {
    if (Exists(from)) {
      typename INTO::Primitives evolved;
      Evolve<FROM, typename INTO::Primitives, EVOLUTOR>::template Go<INTO>(Value(from), evolved);
      into = evolved;
    } else {
      into = nullptr;
    }
  }
};

// Default evolution for `Optional<std::vector<A>>`.
template <typename FROM, typename EVOLUTOR>
struct Evolve<FROM, Optional<std::vector<typename USERSPACE_15964E7953DBD27D::A>>, EVOLUTOR> {
  template <typename INTO, typename INTO_TYPE>
  static void Go(const Optional<std::vector<typename USERSPACE_15964E7953DBD27D::A>>& from, INTO_TYPE& into) {
    if (Exists(from)) {
      std::vector<typename INTO::A> evolved;
      Evolve<FROM, std::vector<typename INTO::A>, EVOLUTOR>::template Go<INTO>(Value(from), evolved);
      into = evolved;
    } else {
      into = nullptr;
    }
  }
};

// Default evolution for `Optional<std::vector<int32_t>>`.
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

// Default evolution for `Optional<std::vector<std::string>>`.
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

// Default evolution for `Variant<A, X, Y>`.
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_15964E7953DBD27D_MyFreakingVariant_Cases {
  DST& into;
  explicit USERSPACE_15964E7953DBD27D_MyFreakingVariant_Cases(DST& into) : into(into) {}
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
struct Evolve<FROM, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_15964E7953DBD27D::A, USERSPACE_15964E7953DBD27D::X, USERSPACE_15964E7953DBD27D::Y>>, EVOLUTOR> {
  // TODO(dkorolev): A `static_assert` to ensure the number of cases is the same.
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_15964E7953DBD27D, CHECK>::value>>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_15964E7953DBD27D::A, USERSPACE_15964E7953DBD27D::X, USERSPACE_15964E7953DBD27D::Y>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(USERSPACE_15964E7953DBD27D_MyFreakingVariant_Cases<decltype(into), FROM, INTO, EVOLUTOR>(into));
  }
};

// Default evolution for `Variant<A, B, B2, C, Empty>`.
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_15964E7953DBD27D_Variant_B_A_B_B2_C_Empty_E_Cases {
  DST& into;
  explicit USERSPACE_15964E7953DBD27D_Variant_B_A_B_B2_C_Empty_E_Cases(DST& into) : into(into) {}
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
struct Evolve<FROM, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_15964E7953DBD27D::A, USERSPACE_15964E7953DBD27D::B, USERSPACE_15964E7953DBD27D::B2, USERSPACE_15964E7953DBD27D::C, USERSPACE_15964E7953DBD27D::Empty>>, EVOLUTOR> {
  // TODO(dkorolev): A `static_assert` to ensure the number of cases is the same.
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE,
            class CHECK = FROM,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_15964E7953DBD27D, CHECK>::value>>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_15964E7953DBD27D::A, USERSPACE_15964E7953DBD27D::B, USERSPACE_15964E7953DBD27D::B2, USERSPACE_15964E7953DBD27D::C, USERSPACE_15964E7953DBD27D::Empty>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(USERSPACE_15964E7953DBD27D_Variant_B_A_B_B2_C_Empty_E_Cases<decltype(into), FROM, INTO, EVOLUTOR>(into));
  }
};

}  // namespace current::type_evolution
}  // namespace current

// Privileged types.
CURRENT_DERIVED_NAMESPACE(ExposedNamespace, USERSPACE_15964E7953DBD27D) {
  CURRENT_NAMESPACE_TYPE(Empty, current_userspace_15964e7953dbd27d::Empty);
  CURRENT_NAMESPACE_TYPE(ExposedFullTest, current_userspace_15964e7953dbd27d::FullTest);
  CURRENT_NAMESPACE_TYPE(Primitives, current_userspace_15964e7953dbd27d::Primitives);
};  // CURRENT_NAMESPACE(ExposedNamespace)

// clang-format on

#endif  // CURRENT_USERSPACE_15964E7953DBD27D
