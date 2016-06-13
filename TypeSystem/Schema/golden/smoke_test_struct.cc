// The `current.h` file is the one from `https://github.com/C5T/Current`.
// Compile with `-std=c++11` or higher.

#ifndef CURRENT_USERSPACE_0473D52B47432E49
#define CURRENT_USERSPACE_0473D52B47432E49

#include "current.h"

// clang-format off

namespace current_userspace_0473d52b47432e49 {
struct Primitives {
  // It's the "order" of fields that matters.
  uint8_t a;

  // Field descriptions can be set in any order.
  uint16_t b;
  uint32_t c;
  uint64_t d;
  int8_t e;
  int16_t f;
  int32_t g;
  int64_t h;
  char i;
  std::string j;
  float k;
  double l;

  // Multiline
  // descriptions
  // can be used.
  bool m;
  std::chrono::microseconds n;
  std::chrono::milliseconds o;
};
struct A {
  int32_t a;
};
struct B : A {
  int32_t b;
};
struct B2 : A {
};
struct Empty {
};
struct X {
  int32_t x;
};
enum class E : uint16_t {};
struct Y {
  E e;
};
using MyFreakingVariant = Variant<A, X, Y>;
using Variant_B_A_X_Y_E = Variant<A, X, Y>;
struct C {
  Empty e;
  MyFreakingVariant c;
  Variant_B_A_X_Y_E d;
};
using Variant_B_A_B_B2_C_Empty_E = Variant<A, B, B2, C, Empty>;
struct Templated_T9209980946934124423 {
  int32_t foo;
  X bar;
};
struct Templated_T9227782344077896555 {
  int32_t foo;
  MyFreakingVariant bar;
};
struct TemplatedInheriting_T9200000002835747520 : A {
  std::string baz;
  Empty meh;
};
struct Templated_T9209626390174323094 {
  int32_t foo;
  TemplatedInheriting_T9200000002835747520 bar;
};
struct TemplatedInheriting_T9209980946934124423 : A {
  std::string baz;
  X meh;
};
struct TemplatedInheriting_T9227782344077896555 : A {
  std::string baz;
  MyFreakingVariant meh;
};
struct Templated_T9200000002835747520 {
  int32_t foo;
  Empty bar;
};
struct TemplatedInheriting_T9201673071807149456 : A {
  std::string baz;
  Templated_T9200000002835747520 meh;
};
struct FullTest {
  // A structure with a lot of primitive types.
  Primitives primitives;
  std::vector<std::string> v1;
  std::vector<Primitives> v2;
  std::pair<std::string, Primitives> p;
  Optional<Primitives> o;

  // Field | descriptions | FTW !
  Variant_B_A_B_B2_C_Empty_E q;
  Templated_T9209980946934124423 w1;
  Templated_T9227782344077896555 w2;
  Templated_T9209626390174323094 w3;
  TemplatedInheriting_T9209980946934124423 w4;
  TemplatedInheriting_T9227782344077896555 w5;
  TemplatedInheriting_T9201673071807149456 w6;
};
}  // namespace current_userspace_0473d52b47432e49

// clang-format on

#endif  // CURRENT_USERSPACE_0473D52B47432E49
