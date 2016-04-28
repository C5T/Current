// The `current.h` file is the one from `https://github.com/C5T/Current`.
// Compile with `-std=c++11` or higher.

#include "current.h"

// clang-format off

namespace current_userspace {
struct Primitives {
  uint8_t a;
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
struct C {
  Empty e;
  VarianT_B_A_X_Y_E c;
};
using Variant_B_A_B_C_Empty_E = Variant<A, B, C, Empty>;
struct FullTest {
  Primitives primitives;
  std::vector<std::string> v1;
  std::vector<Primitives> v2;
  std::pair<std::string, Primitives> p;
  Optional<Primitives> o;
  VarianT_B_A_B_C_Empty_E q;
};
}  // namespace current_userspace

// clang-format on
