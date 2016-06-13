// The `current.h` file is the one from `https://github.com/C5T/Current`.
// Compile with `-std=c++11` or higher.

#ifndef CURRENT_USERSPACE_0473D52B47432E49
#define CURRENT_USERSPACE_0473D52B47432E49

#include "current.h"

// clang-format off

namespace current_userspace_0473d52b47432e49 {

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
};
using T9209385642811445282 = FullTest;

}  // namespace current_userspace_0473d52b47432e49

CURRENT_NAMESPACE(USERSPACE_0473D52B47432E49) {
  CURRENT_NAMESPACE_TYPE(E, current_userspace_0473d52b47432e49::E);
  CURRENT_NAMESPACE_TYPE(Empty, current_userspace_0473d52b47432e49::Empty);
  CURRENT_NAMESPACE_TYPE(B, current_userspace_0473d52b47432e49::B);
  CURRENT_NAMESPACE_TYPE(Templated_T9209626390174323094, current_userspace_0473d52b47432e49::Templated_T9209626390174323094);
  CURRENT_NAMESPACE_TYPE(Templated_T9200000002835747520, current_userspace_0473d52b47432e49::Templated_T9200000002835747520);
  CURRENT_NAMESPACE_TYPE(C, current_userspace_0473d52b47432e49::C);
  CURRENT_NAMESPACE_TYPE(TemplatedInheriting_T9201673071807149456, current_userspace_0473d52b47432e49::TemplatedInheriting_T9201673071807149456);
  CURRENT_NAMESPACE_TYPE(A, current_userspace_0473d52b47432e49::A);
  CURRENT_NAMESPACE_TYPE(Primitives, current_userspace_0473d52b47432e49::Primitives);
  CURRENT_NAMESPACE_TYPE(TemplatedInheriting_T9209980946934124423, current_userspace_0473d52b47432e49::TemplatedInheriting_T9209980946934124423);
  CURRENT_NAMESPACE_TYPE(Y, current_userspace_0473d52b47432e49::Y);
  CURRENT_NAMESPACE_TYPE(FullTest, current_userspace_0473d52b47432e49::FullTest);
  CURRENT_NAMESPACE_TYPE(Templated_T9209980946934124423, current_userspace_0473d52b47432e49::Templated_T9209980946934124423);
  CURRENT_NAMESPACE_TYPE(TemplatedInheriting_T9227782344077896555, current_userspace_0473d52b47432e49::TemplatedInheriting_T9227782344077896555);
  CURRENT_NAMESPACE_TYPE(TemplatedInheriting_T9200000002835747520, current_userspace_0473d52b47432e49::TemplatedInheriting_T9200000002835747520);
  CURRENT_NAMESPACE_TYPE(B2, current_userspace_0473d52b47432e49::B2);
  CURRENT_NAMESPACE_TYPE(Templated_T9227782344077896555, current_userspace_0473d52b47432e49::Templated_T9227782344077896555);
  CURRENT_NAMESPACE_TYPE(X, current_userspace_0473d52b47432e49::X);
  CURRENT_NAMESPACE_TYPE(MyFreakingVariant, current_userspace_0473d52b47432e49::MyFreakingVariant);
  CURRENT_NAMESPACE_TYPE(Variant_B_A_X_Y_E, current_userspace_0473d52b47432e49::Variant_B_A_X_Y_E);
  CURRENT_NAMESPACE_TYPE(Variant_B_A_B_B2_C_Empty_E, current_userspace_0473d52b47432e49::Variant_B_A_B_B2_C_Empty_E);
};

namespace current {
namespace type_evolution {

// Default evolution for `CURRENT_ENUM(E)`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::E, EVOLUTOR> {
  template <typename INTO>
  static void Go(USERSPACE_0473D52B47432E49::E from,
                 typename INTO::E& into) {
    // TODO(dkorolev): Check enum underlying type, but not too strictly to be extensible.
    into = static_cast<typename INTO::E>(from);
  }
};

// Default evolution for struct `Empty`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::Empty, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_0473D52B47432E49, CHECK>::value>>
  static void Go(const typename NAMESPACE::Empty& from,
                 typename INTO::Empty& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_0473D52B47432E49::Empty>::value == 0,
                    "Custom evolutor required.");
      static_cast<void>(from);
      static_cast<void>(into);
  }
};

// Default evolution for struct `B`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::B, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_0473D52B47432E49, CHECK>::value>>
  static void Go(const typename NAMESPACE::B& from,
                 typename INTO::B& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_0473D52B47432E49::B>::value == 1,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::A, EVOLUTOR>::template Go<INTO>(static_cast<const typename NAMESPACE::A&>(from), static_cast<typename INTO::A&>(into));
      Evolve<NAMESPACE, decltype(from.b), EVOLUTOR>::template Go<INTO>(from.b, into.b);
  }
};

// Default evolution for struct `Templated_T9209626390174323094`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::Templated_T9209626390174323094, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_0473D52B47432E49, CHECK>::value>>
  static void Go(const typename NAMESPACE::Templated_T9209626390174323094& from,
                 typename INTO::Templated_T9209626390174323094& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_0473D52B47432E49::Templated_T9209626390174323094>::value == 2,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.foo), EVOLUTOR>::template Go<INTO>(from.foo, into.foo);
      Evolve<NAMESPACE, decltype(from.bar), EVOLUTOR>::template Go<INTO>(from.bar, into.bar);
  }
};

// Default evolution for struct `Templated_T9200000002835747520`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::Templated_T9200000002835747520, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_0473D52B47432E49, CHECK>::value>>
  static void Go(const typename NAMESPACE::Templated_T9200000002835747520& from,
                 typename INTO::Templated_T9200000002835747520& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_0473D52B47432E49::Templated_T9200000002835747520>::value == 2,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.foo), EVOLUTOR>::template Go<INTO>(from.foo, into.foo);
      Evolve<NAMESPACE, decltype(from.bar), EVOLUTOR>::template Go<INTO>(from.bar, into.bar);
  }
};

// Default evolution for struct `C`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::C, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_0473D52B47432E49, CHECK>::value>>
  static void Go(const typename NAMESPACE::C& from,
                 typename INTO::C& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_0473D52B47432E49::C>::value == 3,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.e), EVOLUTOR>::template Go<INTO>(from.e, into.e);
      Evolve<NAMESPACE, decltype(from.c), EVOLUTOR>::template Go<INTO>(from.c, into.c);
      Evolve<NAMESPACE, decltype(from.d), EVOLUTOR>::template Go<INTO>(from.d, into.d);
  }
};

// Default evolution for struct `TemplatedInheriting_T9201673071807149456`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::TemplatedInheriting_T9201673071807149456, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_0473D52B47432E49, CHECK>::value>>
  static void Go(const typename NAMESPACE::TemplatedInheriting_T9201673071807149456& from,
                 typename INTO::TemplatedInheriting_T9201673071807149456& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_0473D52B47432E49::TemplatedInheriting_T9201673071807149456>::value == 2,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::A, EVOLUTOR>::template Go<INTO>(static_cast<const typename NAMESPACE::A&>(from), static_cast<typename INTO::A&>(into));
      Evolve<NAMESPACE, decltype(from.baz), EVOLUTOR>::template Go<INTO>(from.baz, into.baz);
      Evolve<NAMESPACE, decltype(from.meh), EVOLUTOR>::template Go<INTO>(from.meh, into.meh);
  }
};

// Default evolution for struct `A`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::A, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_0473D52B47432E49, CHECK>::value>>
  static void Go(const typename NAMESPACE::A& from,
                 typename INTO::A& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_0473D52B47432E49::A>::value == 1,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.a), EVOLUTOR>::template Go<INTO>(from.a, into.a);
  }
};

// Default evolution for struct `Primitives`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::Primitives, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_0473D52B47432E49, CHECK>::value>>
  static void Go(const typename NAMESPACE::Primitives& from,
                 typename INTO::Primitives& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_0473D52B47432E49::Primitives>::value == 15,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.a), EVOLUTOR>::template Go<INTO>(from.a, into.a);
      Evolve<NAMESPACE, decltype(from.b), EVOLUTOR>::template Go<INTO>(from.b, into.b);
      Evolve<NAMESPACE, decltype(from.c), EVOLUTOR>::template Go<INTO>(from.c, into.c);
      Evolve<NAMESPACE, decltype(from.d), EVOLUTOR>::template Go<INTO>(from.d, into.d);
      Evolve<NAMESPACE, decltype(from.e), EVOLUTOR>::template Go<INTO>(from.e, into.e);
      Evolve<NAMESPACE, decltype(from.f), EVOLUTOR>::template Go<INTO>(from.f, into.f);
      Evolve<NAMESPACE, decltype(from.g), EVOLUTOR>::template Go<INTO>(from.g, into.g);
      Evolve<NAMESPACE, decltype(from.h), EVOLUTOR>::template Go<INTO>(from.h, into.h);
      Evolve<NAMESPACE, decltype(from.i), EVOLUTOR>::template Go<INTO>(from.i, into.i);
      Evolve<NAMESPACE, decltype(from.j), EVOLUTOR>::template Go<INTO>(from.j, into.j);
      Evolve<NAMESPACE, decltype(from.k), EVOLUTOR>::template Go<INTO>(from.k, into.k);
      Evolve<NAMESPACE, decltype(from.l), EVOLUTOR>::template Go<INTO>(from.l, into.l);
      Evolve<NAMESPACE, decltype(from.m), EVOLUTOR>::template Go<INTO>(from.m, into.m);
      Evolve<NAMESPACE, decltype(from.n), EVOLUTOR>::template Go<INTO>(from.n, into.n);
      Evolve<NAMESPACE, decltype(from.o), EVOLUTOR>::template Go<INTO>(from.o, into.o);
  }
};

// Default evolution for struct `TemplatedInheriting_T9209980946934124423`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::TemplatedInheriting_T9209980946934124423, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_0473D52B47432E49, CHECK>::value>>
  static void Go(const typename NAMESPACE::TemplatedInheriting_T9209980946934124423& from,
                 typename INTO::TemplatedInheriting_T9209980946934124423& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_0473D52B47432E49::TemplatedInheriting_T9209980946934124423>::value == 2,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::A, EVOLUTOR>::template Go<INTO>(static_cast<const typename NAMESPACE::A&>(from), static_cast<typename INTO::A&>(into));
      Evolve<NAMESPACE, decltype(from.baz), EVOLUTOR>::template Go<INTO>(from.baz, into.baz);
      Evolve<NAMESPACE, decltype(from.meh), EVOLUTOR>::template Go<INTO>(from.meh, into.meh);
  }
};

// Default evolution for struct `Y`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::Y, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_0473D52B47432E49, CHECK>::value>>
  static void Go(const typename NAMESPACE::Y& from,
                 typename INTO::Y& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_0473D52B47432E49::Y>::value == 1,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.e), EVOLUTOR>::template Go<INTO>(from.e, into.e);
  }
};

// Default evolution for struct `FullTest`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::FullTest, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_0473D52B47432E49, CHECK>::value>>
  static void Go(const typename NAMESPACE::FullTest& from,
                 typename INTO::FullTest& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_0473D52B47432E49::FullTest>::value == 12,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.primitives), EVOLUTOR>::template Go<INTO>(from.primitives, into.primitives);
      Evolve<NAMESPACE, decltype(from.v1), EVOLUTOR>::template Go<INTO>(from.v1, into.v1);
      Evolve<NAMESPACE, decltype(from.v2), EVOLUTOR>::template Go<INTO>(from.v2, into.v2);
      Evolve<NAMESPACE, decltype(from.p), EVOLUTOR>::template Go<INTO>(from.p, into.p);
      Evolve<NAMESPACE, decltype(from.o), EVOLUTOR>::template Go<INTO>(from.o, into.o);
      Evolve<NAMESPACE, decltype(from.q), EVOLUTOR>::template Go<INTO>(from.q, into.q);
      Evolve<NAMESPACE, decltype(from.w1), EVOLUTOR>::template Go<INTO>(from.w1, into.w1);
      Evolve<NAMESPACE, decltype(from.w2), EVOLUTOR>::template Go<INTO>(from.w2, into.w2);
      Evolve<NAMESPACE, decltype(from.w3), EVOLUTOR>::template Go<INTO>(from.w3, into.w3);
      Evolve<NAMESPACE, decltype(from.w4), EVOLUTOR>::template Go<INTO>(from.w4, into.w4);
      Evolve<NAMESPACE, decltype(from.w5), EVOLUTOR>::template Go<INTO>(from.w5, into.w5);
      Evolve<NAMESPACE, decltype(from.w6), EVOLUTOR>::template Go<INTO>(from.w6, into.w6);
  }
};

// Default evolution for struct `Templated_T9209980946934124423`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::Templated_T9209980946934124423, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_0473D52B47432E49, CHECK>::value>>
  static void Go(const typename NAMESPACE::Templated_T9209980946934124423& from,
                 typename INTO::Templated_T9209980946934124423& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_0473D52B47432E49::Templated_T9209980946934124423>::value == 2,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.foo), EVOLUTOR>::template Go<INTO>(from.foo, into.foo);
      Evolve<NAMESPACE, decltype(from.bar), EVOLUTOR>::template Go<INTO>(from.bar, into.bar);
  }
};

// Default evolution for struct `TemplatedInheriting_T9227782344077896555`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::TemplatedInheriting_T9227782344077896555, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_0473D52B47432E49, CHECK>::value>>
  static void Go(const typename NAMESPACE::TemplatedInheriting_T9227782344077896555& from,
                 typename INTO::TemplatedInheriting_T9227782344077896555& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_0473D52B47432E49::TemplatedInheriting_T9227782344077896555>::value == 2,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::A, EVOLUTOR>::template Go<INTO>(static_cast<const typename NAMESPACE::A&>(from), static_cast<typename INTO::A&>(into));
      Evolve<NAMESPACE, decltype(from.baz), EVOLUTOR>::template Go<INTO>(from.baz, into.baz);
      Evolve<NAMESPACE, decltype(from.meh), EVOLUTOR>::template Go<INTO>(from.meh, into.meh);
  }
};

// Default evolution for struct `TemplatedInheriting_T9200000002835747520`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::TemplatedInheriting_T9200000002835747520, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_0473D52B47432E49, CHECK>::value>>
  static void Go(const typename NAMESPACE::TemplatedInheriting_T9200000002835747520& from,
                 typename INTO::TemplatedInheriting_T9200000002835747520& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_0473D52B47432E49::TemplatedInheriting_T9200000002835747520>::value == 2,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::A, EVOLUTOR>::template Go<INTO>(static_cast<const typename NAMESPACE::A&>(from), static_cast<typename INTO::A&>(into));
      Evolve<NAMESPACE, decltype(from.baz), EVOLUTOR>::template Go<INTO>(from.baz, into.baz);
      Evolve<NAMESPACE, decltype(from.meh), EVOLUTOR>::template Go<INTO>(from.meh, into.meh);
  }
};

// Default evolution for struct `B2`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::B2, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_0473D52B47432E49, CHECK>::value>>
  static void Go(const typename NAMESPACE::B2& from,
                 typename INTO::B2& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_0473D52B47432E49::B2>::value == 0,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::A, EVOLUTOR>::template Go<INTO>(static_cast<const typename NAMESPACE::A&>(from), static_cast<typename INTO::A&>(into));
      static_cast<void>(from);
      static_cast<void>(into);
  }
};

// Default evolution for struct `Templated_T9227782344077896555`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::Templated_T9227782344077896555, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_0473D52B47432E49, CHECK>::value>>
  static void Go(const typename NAMESPACE::Templated_T9227782344077896555& from,
                 typename INTO::Templated_T9227782344077896555& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_0473D52B47432E49::Templated_T9227782344077896555>::value == 2,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.foo), EVOLUTOR>::template Go<INTO>(from.foo, into.foo);
      Evolve<NAMESPACE, decltype(from.bar), EVOLUTOR>::template Go<INTO>(from.bar, into.bar);
  }
};

// Default evolution for struct `X`.
template <typename NAMESPACE, typename EVOLUTOR>
struct Evolve<NAMESPACE, USERSPACE_0473D52B47432E49::X, EVOLUTOR> {
  template <typename INTO,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_0473D52B47432E49, CHECK>::value>>
  static void Go(const typename NAMESPACE::X& from,
                 typename INTO::X& into) {
      static_assert(::current::reflection::FieldCounter<typename USERSPACE_0473D52B47432E49::X>::value == 1,
                    "Custom evolutor required.");
      Evolve<NAMESPACE, decltype(from.x), EVOLUTOR>::template Go<INTO>(from.x, into.x);
  }
};

// Default evolution for `Variant<A, X, Y>`.
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_0473D52B47432E49_MyFreakingVariant_Cases {
  DST& into;
  explicit USERSPACE_0473D52B47432E49_MyFreakingVariant_Cases(DST& into) : into(into) {}
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
template <typename NAMESPACE, typename EVOLUTOR, typename VARIANT_NAME_HELPER>
struct Evolve<NAMESPACE, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_0473D52B47432E49::A, USERSPACE_0473D52B47432E49::X, USERSPACE_0473D52B47432E49::Y>>, EVOLUTOR> {
  // TODO(dkorolev): A `static_assert` to ensure the number of cases is the same.
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_0473D52B47432E49, CHECK>::value>>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_0473D52B47432E49::A, USERSPACE_0473D52B47432E49::X, USERSPACE_0473D52B47432E49::Y>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(USERSPACE_0473D52B47432E49_MyFreakingVariant_Cases<decltype(into), NAMESPACE, INTO, EVOLUTOR>(into));
  }
};

// Default evolution for `Variant<A, B, B2, C, Empty>`.
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_0473D52B47432E49_Variant_B_A_B_B2_C_Empty_E_Cases {
  DST& into;
  explicit USERSPACE_0473D52B47432E49_Variant_B_A_B_B2_C_Empty_E_Cases(DST& into) : into(into) {}
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
template <typename NAMESPACE, typename EVOLUTOR, typename VARIANT_NAME_HELPER>
struct Evolve<NAMESPACE, ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_0473D52B47432E49::A, USERSPACE_0473D52B47432E49::B, USERSPACE_0473D52B47432E49::B2, USERSPACE_0473D52B47432E49::C, USERSPACE_0473D52B47432E49::Empty>>, EVOLUTOR> {
  // TODO(dkorolev): A `static_assert` to ensure the number of cases is the same.
  template <typename INTO,
            typename CUSTOM_INTO_VARIANT_TYPE,
            class CHECK = NAMESPACE,
            class = std::enable_if_t<::current::is_same_or_base_of<USERSPACE_0473D52B47432E49, CHECK>::value>>
  static void Go(const ::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<USERSPACE_0473D52B47432E49::A, USERSPACE_0473D52B47432E49::B, USERSPACE_0473D52B47432E49::B2, USERSPACE_0473D52B47432E49::C, USERSPACE_0473D52B47432E49::Empty>>& from,
                 CUSTOM_INTO_VARIANT_TYPE& into) {
    from.Call(USERSPACE_0473D52B47432E49_Variant_B_A_B_B2_C_Empty_E_Cases<decltype(into), NAMESPACE, INTO, EVOLUTOR>(into));
  }
};

}  // namespace current::type_evolution
}  // namespace current

// clang-format on

#endif  // CURRENT_USERSPACE_0473D52B47432E49
