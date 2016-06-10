// The `current.h` file is the one from `https://github.com/C5T/Current`.
// Compile with `-std=c++11` or higher.

#ifndef CURRENT_USERSPACE_504F3B41CF471A78
#define CURRENT_USERSPACE_504F3B41CF471A78

#include "current.h"

// clang-format off

namespace current_userspace_504f3b41cf471a78 {

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
using T9201269004751425979 = B;

CURRENT_STRUCT(B2, A) {
};
using T9206911750331566526 = B2;

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

CURRENT_STRUCT(C) {
  CURRENT_FIELD(e, Empty);
  CURRENT_FIELD(c, MyFreakingVariant);
};
using T9204551010916892864 = C;

CURRENT_VARIANT(Variant_B_A_B_B2_C_Empty_E, A, B, B2, C, Empty);
using T9222925463038817794 = Variant_B_A_B_B2_C_Empty_E;

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
using T9205138582037435887 = TemplatedInheriting_T9200000002835747520;

CURRENT_STRUCT(Templated_T9205138582037435887) {
  CURRENT_FIELD(foo, int32_t);
  CURRENT_FIELD(bar, TemplatedInheriting_T9200000002835747520);
};
using T9200217711470831804 = Templated_T9205138582037435887;

CURRENT_STRUCT(TemplatedInheriting_T9209980946934124423, A) {
  CURRENT_FIELD(baz, std::string);
  CURRENT_FIELD(meh, X);
};
using T9204618444014675930 = TemplatedInheriting_T9209980946934124423;

CURRENT_STRUCT(TemplatedInheriting_T9227782344077896555, A) {
  CURRENT_FIELD(baz, std::string);
  CURRENT_FIELD(meh, MyFreakingVariant);
};
using T9204359772649076736 = TemplatedInheriting_T9227782344077896555;

CURRENT_STRUCT(Templated_T9200000002835747520) {
  CURRENT_FIELD(foo, int32_t);
  CURRENT_FIELD(bar, Empty);
};
using T9201673071807149456 = Templated_T9200000002835747520;

CURRENT_STRUCT(TemplatedInheriting_T9201673071807149456, A) {
  CURRENT_FIELD(baz, std::string);
  CURRENT_FIELD(meh, Templated_T9200000002835747520);
};
using T9209233353985188699 = TemplatedInheriting_T9201673071807149456;

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
  CURRENT_FIELD(w3, Templated_T9205138582037435887);
  CURRENT_FIELD(w4, TemplatedInheriting_T9209980946934124423);
  CURRENT_FIELD(w5, TemplatedInheriting_T9227782344077896555);
  CURRENT_FIELD(w6, TemplatedInheriting_T9201673071807149456);
};
using T9203579289283591161 = FullTest;

}  // namespace current_userspace_504f3b41cf471a78

CURRENT_NAMESPACE(USERSPACE_504F3B41CF471A78) {
  CURRENT_NAMESPACE_TYPE(E, current_userspace_504f3b41cf471a78::E);
  CURRENT_NAMESPACE_TYPE(Empty, current_userspace_504f3b41cf471a78::Empty);
  CURRENT_NAMESPACE_TYPE(Templated_T9205138582037435887, current_userspace_504f3b41cf471a78::Templated_T9205138582037435887);
  CURRENT_NAMESPACE_TYPE(B, current_userspace_504f3b41cf471a78::B);
  CURRENT_NAMESPACE_TYPE(Templated_T9200000002835747520, current_userspace_504f3b41cf471a78::Templated_T9200000002835747520);
  CURRENT_NAMESPACE_TYPE(FullTest, current_userspace_504f3b41cf471a78::FullTest);
  CURRENT_NAMESPACE_TYPE(TemplatedInheriting_T9227782344077896555, current_userspace_504f3b41cf471a78::TemplatedInheriting_T9227782344077896555);
  CURRENT_NAMESPACE_TYPE(C, current_userspace_504f3b41cf471a78::C);
  CURRENT_NAMESPACE_TYPE(TemplatedInheriting_T9209980946934124423, current_userspace_504f3b41cf471a78::TemplatedInheriting_T9209980946934124423);
  CURRENT_NAMESPACE_TYPE(TemplatedInheriting_T9200000002835747520, current_userspace_504f3b41cf471a78::TemplatedInheriting_T9200000002835747520);
  CURRENT_NAMESPACE_TYPE(A, current_userspace_504f3b41cf471a78::A);
  CURRENT_NAMESPACE_TYPE(B2, current_userspace_504f3b41cf471a78::B2);
  CURRENT_NAMESPACE_TYPE(Primitives, current_userspace_504f3b41cf471a78::Primitives);
  CURRENT_NAMESPACE_TYPE(Y, current_userspace_504f3b41cf471a78::Y);
  CURRENT_NAMESPACE_TYPE(TemplatedInheriting_T9201673071807149456, current_userspace_504f3b41cf471a78::TemplatedInheriting_T9201673071807149456);
  CURRENT_NAMESPACE_TYPE(Templated_T9209980946934124423, current_userspace_504f3b41cf471a78::Templated_T9209980946934124423);
  CURRENT_NAMESPACE_TYPE(Templated_T9227782344077896555, current_userspace_504f3b41cf471a78::Templated_T9227782344077896555);
  CURRENT_NAMESPACE_TYPE(X, current_userspace_504f3b41cf471a78::X);
  CURRENT_NAMESPACE_TYPE(Variant_B_A_B_B2_C_Empty_E, current_userspace_504f3b41cf471a78::Variant_B_A_B_B2_C_Empty_E);
  CURRENT_NAMESPACE_TYPE(MyFreakingVariant, current_userspace_504f3b41cf471a78::MyFreakingVariant);
};

namespace current {
namespace type_evolution {
// Default evolution for `Empty`.
template <typename EVOLUTOR>
struct Evolve<USERSPACE_504F3B41CF471A78, USERSPACE_504F3B41CF471A78::Empty, EVOLUTOR> {
  template <typename INTO>
  static void Go(const typename USERSPACE_504F3B41CF471A78::Empty& from,
                 typename INTO::Empty& into) {
      static_assert(::current::reflection::TotalFieldCounter<typename USERSPACE_504F3B41CF471A78::Empty>::value == 0,
                    "Custom evolutor required.");
      static_cast<void>(from);
      static_cast<void>(into);
  }
};

// Default evolution for `Templated_T9205138582037435887`.
template <typename EVOLUTOR>
struct Evolve<USERSPACE_504F3B41CF471A78, USERSPACE_504F3B41CF471A78::Templated_T9205138582037435887, EVOLUTOR> {
  template <typename INTO>
  static void Go(const typename USERSPACE_504F3B41CF471A78::Templated_T9205138582037435887& from,
                 typename INTO::Templated_T9205138582037435887& into) {
      static_assert(::current::reflection::TotalFieldCounter<typename USERSPACE_504F3B41CF471A78::Templated_T9205138582037435887>::value == 2,
                    "Custom evolutor required.");
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.foo), EVOLUTOR>::template Go<INTO>(from.foo, into.foo);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.bar), EVOLUTOR>::template Go<INTO>(from.bar, into.bar);
  }
};

// Default evolution for `B`.
template <typename EVOLUTOR>
struct Evolve<USERSPACE_504F3B41CF471A78, USERSPACE_504F3B41CF471A78::B, EVOLUTOR> {
  template <typename INTO>
  static void Go(const typename USERSPACE_504F3B41CF471A78::B& from,
                 typename INTO::B& into) {
      static_assert(::current::reflection::TotalFieldCounter<typename USERSPACE_504F3B41CF471A78::B>::value == 2,
                    "Custom evolutor required.");
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.a), EVOLUTOR>::template Go<INTO>(from.a, into.a);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.b), EVOLUTOR>::template Go<INTO>(from.b, into.b);
  }
};

// Default evolution for `Templated_T9200000002835747520`.
template <typename EVOLUTOR>
struct Evolve<USERSPACE_504F3B41CF471A78, USERSPACE_504F3B41CF471A78::Templated_T9200000002835747520, EVOLUTOR> {
  template <typename INTO>
  static void Go(const typename USERSPACE_504F3B41CF471A78::Templated_T9200000002835747520& from,
                 typename INTO::Templated_T9200000002835747520& into) {
      static_assert(::current::reflection::TotalFieldCounter<typename USERSPACE_504F3B41CF471A78::Templated_T9200000002835747520>::value == 2,
                    "Custom evolutor required.");
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.foo), EVOLUTOR>::template Go<INTO>(from.foo, into.foo);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.bar), EVOLUTOR>::template Go<INTO>(from.bar, into.bar);
  }
};

// Default evolution for `FullTest`.
// TODO(dkorolev): A `static_assert` to ensure the number of cases is the same.
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_504F3B41CF471A78_FullTest_q_Cases {
  DST& into;
  explicit USERSPACE_504F3B41CF471A78_FullTest_q_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::A& value) {
    using into_t = typename INTO::A;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::A, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::B& value) {
    using into_t = typename INTO::B;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::B, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::B2& value) {
    using into_t = typename INTO::B2;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::B2, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::C& value) {
    using into_t = typename INTO::C;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::C, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::Empty& value) {
    using into_t = typename INTO::Empty;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::Empty, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename EVOLUTOR>
struct Evolve<USERSPACE_504F3B41CF471A78, USERSPACE_504F3B41CF471A78::FullTest, EVOLUTOR> {
  template <typename INTO>
  static void Go(const typename USERSPACE_504F3B41CF471A78::FullTest& from,
                 typename INTO::FullTest& into) {
      static_assert(::current::reflection::TotalFieldCounter<typename USERSPACE_504F3B41CF471A78::FullTest>::value == 12,
                    "Custom evolutor required.");
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.primitives), EVOLUTOR>::template Go<INTO>(from.primitives, into.primitives);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.v1), EVOLUTOR>::template Go<INTO>(from.v1, into.v1);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.v2), EVOLUTOR>::template Go<INTO>(from.v2, into.v2);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.p), EVOLUTOR>::template Go<INTO>(from.p, into.p);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.o), EVOLUTOR>::template Go<INTO>(from.o, into.o);
      { USERSPACE_504F3B41CF471A78_FullTest_q_Cases<decltype(into.q), USERSPACE_504F3B41CF471A78, INTO, EVOLUTOR> logic(into.q); from.q.Call(logic); }
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.w1), EVOLUTOR>::template Go<INTO>(from.w1, into.w1);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.w2), EVOLUTOR>::template Go<INTO>(from.w2, into.w2);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.w3), EVOLUTOR>::template Go<INTO>(from.w3, into.w3);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.w4), EVOLUTOR>::template Go<INTO>(from.w4, into.w4);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.w5), EVOLUTOR>::template Go<INTO>(from.w5, into.w5);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.w6), EVOLUTOR>::template Go<INTO>(from.w6, into.w6);
  }
};

// Default evolution for `TemplatedInheriting_T9227782344077896555`.
// TODO(dkorolev): A `static_assert` to ensure the number of cases is the same.
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_504F3B41CF471A78_TemplatedInheriting_T9227782344077896555_meh_Cases {
  DST& into;
  explicit USERSPACE_504F3B41CF471A78_TemplatedInheriting_T9227782344077896555_meh_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::A& value) {
    using into_t = typename INTO::A;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::A, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::X& value) {
    using into_t = typename INTO::X;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::X, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::Y& value) {
    using into_t = typename INTO::Y;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::Y, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename EVOLUTOR>
struct Evolve<USERSPACE_504F3B41CF471A78, USERSPACE_504F3B41CF471A78::TemplatedInheriting_T9227782344077896555, EVOLUTOR> {
  template <typename INTO>
  static void Go(const typename USERSPACE_504F3B41CF471A78::TemplatedInheriting_T9227782344077896555& from,
                 typename INTO::TemplatedInheriting_T9227782344077896555& into) {
      static_assert(::current::reflection::TotalFieldCounter<typename USERSPACE_504F3B41CF471A78::TemplatedInheriting_T9227782344077896555>::value == 3,
                    "Custom evolutor required.");
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.a), EVOLUTOR>::template Go<INTO>(from.a, into.a);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.baz), EVOLUTOR>::template Go<INTO>(from.baz, into.baz);
      { USERSPACE_504F3B41CF471A78_TemplatedInheriting_T9227782344077896555_meh_Cases<decltype(into.meh), USERSPACE_504F3B41CF471A78, INTO, EVOLUTOR> logic(into.meh); from.meh.Call(logic); }
  }
};

// Default evolution for `C`.
// TODO(dkorolev): A `static_assert` to ensure the number of cases is the same.
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_504F3B41CF471A78_C_c_Cases {
  DST& into;
  explicit USERSPACE_504F3B41CF471A78_C_c_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::A& value) {
    using into_t = typename INTO::A;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::A, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::X& value) {
    using into_t = typename INTO::X;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::X, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::Y& value) {
    using into_t = typename INTO::Y;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::Y, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename EVOLUTOR>
struct Evolve<USERSPACE_504F3B41CF471A78, USERSPACE_504F3B41CF471A78::C, EVOLUTOR> {
  template <typename INTO>
  static void Go(const typename USERSPACE_504F3B41CF471A78::C& from,
                 typename INTO::C& into) {
      static_assert(::current::reflection::TotalFieldCounter<typename USERSPACE_504F3B41CF471A78::C>::value == 2,
                    "Custom evolutor required.");
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.e), EVOLUTOR>::template Go<INTO>(from.e, into.e);
      { USERSPACE_504F3B41CF471A78_C_c_Cases<decltype(into.c), USERSPACE_504F3B41CF471A78, INTO, EVOLUTOR> logic(into.c); from.c.Call(logic); }
  }
};

// Default evolution for `TemplatedInheriting_T9209980946934124423`.
template <typename EVOLUTOR>
struct Evolve<USERSPACE_504F3B41CF471A78, USERSPACE_504F3B41CF471A78::TemplatedInheriting_T9209980946934124423, EVOLUTOR> {
  template <typename INTO>
  static void Go(const typename USERSPACE_504F3B41CF471A78::TemplatedInheriting_T9209980946934124423& from,
                 typename INTO::TemplatedInheriting_T9209980946934124423& into) {
      static_assert(::current::reflection::TotalFieldCounter<typename USERSPACE_504F3B41CF471A78::TemplatedInheriting_T9209980946934124423>::value == 3,
                    "Custom evolutor required.");
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.a), EVOLUTOR>::template Go<INTO>(from.a, into.a);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.baz), EVOLUTOR>::template Go<INTO>(from.baz, into.baz);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.meh), EVOLUTOR>::template Go<INTO>(from.meh, into.meh);
  }
};

// Default evolution for `TemplatedInheriting_T9200000002835747520`.
template <typename EVOLUTOR>
struct Evolve<USERSPACE_504F3B41CF471A78, USERSPACE_504F3B41CF471A78::TemplatedInheriting_T9200000002835747520, EVOLUTOR> {
  template <typename INTO>
  static void Go(const typename USERSPACE_504F3B41CF471A78::TemplatedInheriting_T9200000002835747520& from,
                 typename INTO::TemplatedInheriting_T9200000002835747520& into) {
      static_assert(::current::reflection::TotalFieldCounter<typename USERSPACE_504F3B41CF471A78::TemplatedInheriting_T9200000002835747520>::value == 3,
                    "Custom evolutor required.");
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.a), EVOLUTOR>::template Go<INTO>(from.a, into.a);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.baz), EVOLUTOR>::template Go<INTO>(from.baz, into.baz);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.meh), EVOLUTOR>::template Go<INTO>(from.meh, into.meh);
  }
};

// Default evolution for `A`.
template <typename EVOLUTOR>
struct Evolve<USERSPACE_504F3B41CF471A78, USERSPACE_504F3B41CF471A78::A, EVOLUTOR> {
  template <typename INTO>
  static void Go(const typename USERSPACE_504F3B41CF471A78::A& from,
                 typename INTO::A& into) {
      static_assert(::current::reflection::TotalFieldCounter<typename USERSPACE_504F3B41CF471A78::A>::value == 1,
                    "Custom evolutor required.");
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.a), EVOLUTOR>::template Go<INTO>(from.a, into.a);
  }
};

// Default evolution for `B2`.
template <typename EVOLUTOR>
struct Evolve<USERSPACE_504F3B41CF471A78, USERSPACE_504F3B41CF471A78::B2, EVOLUTOR> {
  template <typename INTO>
  static void Go(const typename USERSPACE_504F3B41CF471A78::B2& from,
                 typename INTO::B2& into) {
      static_assert(::current::reflection::TotalFieldCounter<typename USERSPACE_504F3B41CF471A78::B2>::value == 1,
                    "Custom evolutor required.");
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.a), EVOLUTOR>::template Go<INTO>(from.a, into.a);
  }
};

// Default evolution for `Primitives`.
template <typename EVOLUTOR>
struct Evolve<USERSPACE_504F3B41CF471A78, USERSPACE_504F3B41CF471A78::Primitives, EVOLUTOR> {
  template <typename INTO>
  static void Go(const typename USERSPACE_504F3B41CF471A78::Primitives& from,
                 typename INTO::Primitives& into) {
      static_assert(::current::reflection::TotalFieldCounter<typename USERSPACE_504F3B41CF471A78::Primitives>::value == 15,
                    "Custom evolutor required.");
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.a), EVOLUTOR>::template Go<INTO>(from.a, into.a);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.b), EVOLUTOR>::template Go<INTO>(from.b, into.b);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.c), EVOLUTOR>::template Go<INTO>(from.c, into.c);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.d), EVOLUTOR>::template Go<INTO>(from.d, into.d);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.e), EVOLUTOR>::template Go<INTO>(from.e, into.e);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.f), EVOLUTOR>::template Go<INTO>(from.f, into.f);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.g), EVOLUTOR>::template Go<INTO>(from.g, into.g);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.h), EVOLUTOR>::template Go<INTO>(from.h, into.h);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.i), EVOLUTOR>::template Go<INTO>(from.i, into.i);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.j), EVOLUTOR>::template Go<INTO>(from.j, into.j);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.k), EVOLUTOR>::template Go<INTO>(from.k, into.k);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.l), EVOLUTOR>::template Go<INTO>(from.l, into.l);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.m), EVOLUTOR>::template Go<INTO>(from.m, into.m);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.n), EVOLUTOR>::template Go<INTO>(from.n, into.n);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.o), EVOLUTOR>::template Go<INTO>(from.o, into.o);
  }
};

// Default evolution for `Y`.
template <typename EVOLUTOR>
struct Evolve<USERSPACE_504F3B41CF471A78, USERSPACE_504F3B41CF471A78::Y, EVOLUTOR> {
  template <typename INTO>
  static void Go(const typename USERSPACE_504F3B41CF471A78::Y& from,
                 typename INTO::Y& into) {
      static_assert(::current::reflection::TotalFieldCounter<typename USERSPACE_504F3B41CF471A78::Y>::value == 1,
                    "Custom evolutor required.");
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.e), EVOLUTOR>::template Go<INTO>(from.e, into.e);
  }
};

// Default evolution for `TemplatedInheriting_T9201673071807149456`.
template <typename EVOLUTOR>
struct Evolve<USERSPACE_504F3B41CF471A78, USERSPACE_504F3B41CF471A78::TemplatedInheriting_T9201673071807149456, EVOLUTOR> {
  template <typename INTO>
  static void Go(const typename USERSPACE_504F3B41CF471A78::TemplatedInheriting_T9201673071807149456& from,
                 typename INTO::TemplatedInheriting_T9201673071807149456& into) {
      static_assert(::current::reflection::TotalFieldCounter<typename USERSPACE_504F3B41CF471A78::TemplatedInheriting_T9201673071807149456>::value == 3,
                    "Custom evolutor required.");
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.a), EVOLUTOR>::template Go<INTO>(from.a, into.a);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.baz), EVOLUTOR>::template Go<INTO>(from.baz, into.baz);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.meh), EVOLUTOR>::template Go<INTO>(from.meh, into.meh);
  }
};

// Default evolution for `Templated_T9209980946934124423`.
template <typename EVOLUTOR>
struct Evolve<USERSPACE_504F3B41CF471A78, USERSPACE_504F3B41CF471A78::Templated_T9209980946934124423, EVOLUTOR> {
  template <typename INTO>
  static void Go(const typename USERSPACE_504F3B41CF471A78::Templated_T9209980946934124423& from,
                 typename INTO::Templated_T9209980946934124423& into) {
      static_assert(::current::reflection::TotalFieldCounter<typename USERSPACE_504F3B41CF471A78::Templated_T9209980946934124423>::value == 2,
                    "Custom evolutor required.");
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.foo), EVOLUTOR>::template Go<INTO>(from.foo, into.foo);
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.bar), EVOLUTOR>::template Go<INTO>(from.bar, into.bar);
  }
};

// Default evolution for `Templated_T9227782344077896555`.
// TODO(dkorolev): A `static_assert` to ensure the number of cases is the same.
template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct USERSPACE_504F3B41CF471A78_Templated_T9227782344077896555_bar_Cases {
  DST& into;
  explicit USERSPACE_504F3B41CF471A78_Templated_T9227782344077896555_bar_Cases(DST& into) : into(into) {}
  void operator()(const typename FROM_NAMESPACE::A& value) {
    using into_t = typename INTO::A;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::A, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::X& value) {
    using into_t = typename INTO::X;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::X, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
  void operator()(const typename FROM_NAMESPACE::Y& value) {
    using into_t = typename INTO::Y;
    into = into_t();
    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::Y, EVOLUTOR>::template Go<INTO>(value, Value<into_t>(into));
  }
};
template <typename EVOLUTOR>
struct Evolve<USERSPACE_504F3B41CF471A78, USERSPACE_504F3B41CF471A78::Templated_T9227782344077896555, EVOLUTOR> {
  template <typename INTO>
  static void Go(const typename USERSPACE_504F3B41CF471A78::Templated_T9227782344077896555& from,
                 typename INTO::Templated_T9227782344077896555& into) {
      static_assert(::current::reflection::TotalFieldCounter<typename USERSPACE_504F3B41CF471A78::Templated_T9227782344077896555>::value == 2,
                    "Custom evolutor required.");
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.foo), EVOLUTOR>::template Go<INTO>(from.foo, into.foo);
      { USERSPACE_504F3B41CF471A78_Templated_T9227782344077896555_bar_Cases<decltype(into.bar), USERSPACE_504F3B41CF471A78, INTO, EVOLUTOR> logic(into.bar); from.bar.Call(logic); }
  }
};

// Default evolution for `X`.
template <typename EVOLUTOR>
struct Evolve<USERSPACE_504F3B41CF471A78, USERSPACE_504F3B41CF471A78::X, EVOLUTOR> {
  template <typename INTO>
  static void Go(const typename USERSPACE_504F3B41CF471A78::X& from,
                 typename INTO::X& into) {
      static_assert(::current::reflection::TotalFieldCounter<typename USERSPACE_504F3B41CF471A78::X>::value == 1,
                    "Custom evolutor required.");
      Evolve<USERSPACE_504F3B41CF471A78, decltype(from.x), EVOLUTOR>::template Go<INTO>(from.x, into.x);
  }
};

}  // namespace current::type_evolution
}  // namespace current

// clang-format on

#endif  // CURRENT_USERSPACE_504F3B41CF471A78
