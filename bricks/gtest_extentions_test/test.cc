#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_bool(actually_run_comparisons, false, "Set to true to actually run, not just compile, the comparisons.");

#include "custom.h"  // This could be in the `test.cc` file, but want to make sure we're `make check`-friendly. -- D.K.

TEST(GtestExtensions, CustomPrinter) {
  using namespace test_gtest_extensions;
  LooksLikeString a;
  LooksLikeString b;
  a.string = "foo";
  b.string = "bar";
  if (FLAGS_actually_run_comparisons) {
    EXPECT_EQ(a, b) << "This should compile.";
  }
}

TEST(GtestExtensions, CustomComparator) {
  using namespace test_gtest_extensions;
  CrossComparableA a;
  CrossComparableB b;
  a.a = "foo";
  b.b = "bar";
  if (FLAGS_actually_run_comparisons) {
    EXPECT_EQ(a, b) << "This should compile.";
  }
  a.a = "ok";
  b.b = "ok";
  if (FLAGS_actually_run_comparisons) {
    EXPECT_NE(a, b) << "This should compile.";
  }
}
