// The `current.h` file is the one from `https://github.com/C5T/Current`.
// Compile with `-std=c++11` or higher.

#include "current.h"

// clang-format off

namespace current_userspace {
CURRENT_STRUCT(Primitives) {
  CURRENT_FIELD(a, uint8_t);
  CURRENT_FIELD(b, uint16_t);
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
  CURRENT_FIELD(n, std::chrono::microseconds);
  CURRENT_FIELD(o, std::chrono::milliseconds);
};
CURRENT_STRUCT(A) {
  CURRENT_FIELD(a, int32_t);
};
CURRENT_STRUCT(B, A) {
  CURRENT_FIELD(b, int32_t);
};
CURRENT_STRUCT(Empty) {
};
CURRENT_STRUCT(X) {
  CURRENT_FIELD(x, int32_t);
};
CURRENT_ENUM(E, uint16_t) {};
CURRENT_STRUCT(Y) {
  CURRENT_FIELD(e, E);
};
using MyFreakingVariant = Variant<A, X, Y>;
CURRENT_STRUCT(C) {
  CURRENT_FIELD(e, Empty);
  CURRENT_FIELD(c, VarianT_B_A_X_Y_E);
};
using Variant_B_A_B_C_Empty_E = Variant<A, B, C, Empty>;
CURRENT_STRUCT(FullTest) {
  CURRENT_FIELD(primitives, Primitives);
  CURRENT_FIELD(v1, std::vector<std::string>);
  CURRENT_FIELD(v2, std::vector<Primitives>);
  CURRENT_FIELD(p, (std::pair<std::string, Primitives>));
  CURRENT_FIELD(o, Optional<Primitives>);
  CURRENT_FIELD(q, VarianT_B_A_B_C_Empty_E);
};
}  // namespace current_userspace

// clang-format on
