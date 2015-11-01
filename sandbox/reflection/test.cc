#include "reflection.h"

#include "../../3rdparty/gtest/gtest-main.h"

namespace reflection_test {
CURRENT_STRUCT(Foo) { CURRENT_FIELD(uint64_t, i); };
CURRENT_STRUCT_CHECK_FIELDS(Foo);
}  // namespace reflection_test

TEST(Reflection, DescribeCppStruct) {
  using namespace reflection_test;
  EXPECT_EQ(
      "struct Foo {\n"
      "  uint64_t i;\n"
      "};\n",
      DescribeCppStruct<Foo>());
}
