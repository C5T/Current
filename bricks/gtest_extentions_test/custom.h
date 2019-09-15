#include "../../3rdparty/gtest/gtest-custom-comparators.h"

namespace test_gtest_extensions {

struct LooksLikeString {
  std::string string;
  bool operator==(LooksLikeString const& rhs) const { return string == rhs.string; }
};
inline void PrintTo(LooksLikeString const& s, std::ostream* os) { *os << "LooksLikeString(\"" << s.string << "\")"; }

struct CrossComparableA {
  std::string a;
};
inline void PrintTo(CrossComparableA const& a, std::ostream* os) { *os << "CrossComparableA(\"" << a.a << "\")"; }

struct CrossComparableB {
  std::string b;
};
inline void PrintTo(CrossComparableB const& b, std::ostream* os) { *os << "CrossComparableB(\"" << b.b << "\")"; }

}  // namespace test_gtest_extensions

namespace gtest {
template <>
struct CustomComparator<::test_gtest_extensions::CrossComparableA, ::test_gtest_extensions::CrossComparableB> {
  static bool EXPECT_EQ_IMPL(test_gtest_extensions::CrossComparableA const& a,
                             test_gtest_extensions::CrossComparableB const& b) {
    return a.a == b.b;
  }
  static bool EXPECT_NE_IMPL(test_gtest_extensions::CrossComparableA const& a,
                             test_gtest_extensions::CrossComparableB const& b) {
    return a.a != b.b;
  }
};
}  // namespace gtest
