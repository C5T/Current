/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

#define CURRENT_MOCK_TIME

#include "karl.h"

#include "service_generator.h"
#include "service_is_prime.h"

#include "../Blocks/HTTP/api.h"

#include "../Sherlock/sherlock.h"

#include "../Bricks/strings/printf.h"

#include "../Bricks/dflags/dflags.h"

#include "../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_int32(karl_generator_test_port, PickPortForUnitTest(), "Local test port for the `generator` service.");
DEFINE_int32(karl_is_prime_test_port, PickPortForUnitTest(), "Local test port for the `is_prime` service.");

TEST(Karl, SmokeGenerator) {
  const karl_unittest::ServiceGenerator generator(FLAGS_karl_generator_test_port, std::chrono::milliseconds(1));
  EXPECT_EQ("{\"index\":99,\"us\":100000000}\t{\"x\":100,\"is_prime\":null,\"is_perfect\":null}\n",
            HTTP(GET(Printf("localhost:%d/numbers?i=99&n=1", FLAGS_karl_generator_test_port))).body);
}

TEST(Karl, SmokeIsPrime) {
  const karl_unittest::ServiceIsPrime is_prime(FLAGS_karl_is_prime_test_port);
  EXPECT_EQ("YES\n", HTTP(GET(Printf("localhost:%d/is_prime?x=2", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("YES\n", HTTP(GET(Printf("localhost:%d/is_prime?x=2017", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("YES\n",
            HTTP(GET(Printf("localhost:%d/is_prime?x=1000000007", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("NO\n", HTTP(GET(Printf("localhost:%d/is_prime?x=-1", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("NO\n", HTTP(GET(Printf("localhost:%d/is_prime?x=0", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("NO\n", HTTP(GET(Printf("localhost:%d/is_prime?x=1", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("NO\n", HTTP(GET(Printf("localhost:%d/is_prime?x=10", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("NO\n", HTTP(GET(Printf("localhost:%d/is_prime?x=1369", FLAGS_karl_is_prime_test_port))).body);
}
