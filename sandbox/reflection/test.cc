#include "reflection.h"

#include "../../3rdparty/gtest/gtest-main.h"

namespace reflection_test {
CURRENT_STRUCT(Foo) { CURRENT_FIELD(uint64_t, i); };
static_assert(CURRENT_STRUCT_IS_VALID(Foo), "Struct `Foo` was not properly declared.");
}  // namespace reflection_test
static_assert(CURRENT_STRUCT_IS_VALID(reflection_test::Foo), "Struct `Foo` was not properly declared.");

TEST(Reflection, DescribeCppStruct) {
  using namespace reflection_test;
  using bricks::reflection::DescribeCppStruct;

  EXPECT_EQ(
      "struct Foo {\n"
      "  uint64_t i;\n"
      "};\n",
      DescribeCppStruct<Foo>());
}
