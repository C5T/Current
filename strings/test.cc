#include "printf.h"

#include "../3party/gtest/gtest.h"
#include "../3party/gtest/gtest-main.h"

using bricks::strings::Printf;

TEST(StringPrintf, SmokeTest) {
  EXPECT_EQ("Test: 42, 'Hello', 0000ABBA", Printf("Test: %d, '%s', %08X", 42, "Hello", 0xabba));
}
