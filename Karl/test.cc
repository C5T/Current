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
#include "service_annotator.h"
#include "service_filter.h"

#include "../Blocks/HTTP/api.h"

#include "../Sherlock/sherlock.h"

#include "../Bricks/strings/printf.h"

#include "../Bricks/dflags/dflags.h"

#include "../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_uint16(karl_test_port, PickPortForUnitTest(), "Local test port for the `Karl` master service.");
DEFINE_uint16(karl_generator_test_port, PickPortForUnitTest(), "Local test port for the `generator` service.");
DEFINE_uint16(karl_is_prime_test_port, PickPortForUnitTest(), "Local test port for the `is_prime` service.");
DEFINE_uint16(karl_annotator_test_port, PickPortForUnitTest(), "Local test port for the `annotator` service.");
DEFINE_uint16(karl_filter_test_port, PickPortForUnitTest(), "Local test port for the `filter` service.");
DEFINE_bool(karl_run_test_forever, false, "Set to `true` to run the Karl test forever.");

TEST(Karl, SmokeGenerator) {
  const current::karl::Karl karl(FLAGS_karl_test_port);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d", FLAGS_karl_test_port));
  const karl_unittest::ServiceGenerator generator(
      FLAGS_karl_generator_test_port, std::chrono::microseconds(1), karl_locator);

  EXPECT_EQ("{\"index\":100,\"us\":100000000}\t{\"x\":100,\"is_prime\":null}\n",
            HTTP(GET(Printf("http://localhost:%d/numbers?i=100&n=1", FLAGS_karl_generator_test_port))).body);

  {
    current::karl::ClaireStatusBase status;
    ASSERT_NO_THROW(
        status = ParseJSON<current::karl::ClaireStatusBase>(
            HTTP(GET(Printf("http://localhost:%d/.current", FLAGS_karl_generator_test_port))).body));
    EXPECT_TRUE(status.registered);
    EXPECT_EQ("generator", status.service);
  }

  {
    current::karl::KarlStatus status;
    ASSERT_NO_THROW(status = ParseJSON<current::karl::KarlStatus>(
                        HTTP(GET(Printf("http://localhost:%d", FLAGS_karl_test_port))).body));
    EXPECT_EQ(1u, status.services.size());
    EXPECT_EQ("generator", status.services[0].service);
  }
}

TEST(Karl, SmokeIsPrime) {
  const current::karl::Karl karl(FLAGS_karl_test_port);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d", FLAGS_karl_test_port));
  const karl_unittest::ServiceIsPrime is_prime(FLAGS_karl_is_prime_test_port, karl_locator);
  EXPECT_EQ("YES\n", HTTP(GET(Printf("http://localhost:%d/is_prime?x=2", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("YES\n",
            HTTP(GET(Printf("http://localhost:%d/is_prime?x=2017", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("YES\n",
            HTTP(GET(Printf("http://localhost:%d/is_prime?x=1000000007", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("NO\n", HTTP(GET(Printf("http://localhost:%d/is_prime?x=-1", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("NO\n", HTTP(GET(Printf("http://localhost:%d/is_prime?x=0", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("NO\n", HTTP(GET(Printf("http://localhost:%d/is_prime?x=1", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("NO\n", HTTP(GET(Printf("http://localhost:%d/is_prime?x=10", FLAGS_karl_is_prime_test_port))).body);
  EXPECT_EQ("NO\n",
            HTTP(GET(Printf("http://localhost:%d/is_prime?x=1369", FLAGS_karl_is_prime_test_port))).body);

  {
    current::karl::ClaireStatusBase status;
    ASSERT_NO_THROW(status = ParseJSON<current::karl::ClaireStatusBase>(
                        HTTP(GET(Printf("http://localhost:%d/.current", FLAGS_karl_is_prime_test_port))).body));
    EXPECT_TRUE(status.registered);
    EXPECT_EQ("is_prime", status.service);
  }

  {
    current::karl::KarlStatus status;
    ASSERT_NO_THROW(status = ParseJSON<current::karl::KarlStatus>(
                        HTTP(GET(Printf("http://localhost:%d", FLAGS_karl_test_port))).body));
    EXPECT_EQ(1u, status.services.size());
    EXPECT_EQ("is_prime", status.services[0].service);
  }
}

TEST(Karl, SmokeAnnotator) {
  const current::karl::Karl karl(FLAGS_karl_test_port);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d", FLAGS_karl_test_port));
  const karl_unittest::ServiceGenerator generator(
      FLAGS_karl_generator_test_port, std::chrono::microseconds(1), karl_locator);
  const karl_unittest::ServiceIsPrime is_prime(FLAGS_karl_is_prime_test_port, karl_locator);
  const karl_unittest::ServiceAnnotator annotator(
      FLAGS_karl_annotator_test_port,
      Printf("http://localhost:%d/numbers", FLAGS_karl_generator_test_port),
      Printf("http://localhost:%d/is_prime", FLAGS_karl_is_prime_test_port),
      karl_locator);
  ASSERT_NO_THROW(const auto x37 = ParseJSON<karl_unittest::Number>(
                      current::strings::Split(HTTP(GET(Printf("http://localhost:%d/annotated?i=37&n=1",
                                                              FLAGS_karl_annotator_test_port))).body,
                                              '\t').back());
                  EXPECT_EQ(37, x37.x);
                  ASSERT_TRUE(Exists(x37.is_prime));
                  EXPECT_TRUE(Value(x37.is_prime)););
  ASSERT_NO_THROW(const auto x39 = ParseJSON<karl_unittest::Number>(
                      current::strings::Split(HTTP(GET(Printf("http://localhost:%d/annotated?i=39&n=1",
                                                              FLAGS_karl_annotator_test_port))).body,
                                              '\t').back());
                  EXPECT_EQ(39, x39.x);
                  ASSERT_TRUE(Exists(x39.is_prime));
                  EXPECT_FALSE(Value(x39.is_prime)););

  {
    current::karl::ClaireStatusBase status;
    ASSERT_NO_THROW(
        status = ParseJSON<current::karl::ClaireStatusBase>(
            HTTP(GET(Printf("http://localhost:%d/.current", FLAGS_karl_annotator_test_port))).body));
    EXPECT_TRUE(status.registered);
    EXPECT_EQ("annotator", status.service);
  }

  {
    current::karl::KarlStatus status;
    ASSERT_NO_THROW(status = ParseJSON<current::karl::KarlStatus>(
                        HTTP(GET(Printf("http://localhost:%d", FLAGS_karl_test_port))).body));
    EXPECT_EQ(3u, status.services.size());
    EXPECT_EQ("annotator", status.services[0].service);
    EXPECT_EQ("generator", status.services[1].service);
    EXPECT_EQ("is_prime", status.services[2].service);
  }
}

TEST(Karl, SmokeFilter) {
  const current::karl::Karl karl(FLAGS_karl_test_port);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d", FLAGS_karl_test_port));
  const karl_unittest::ServiceGenerator generator(
      FLAGS_karl_generator_test_port, std::chrono::microseconds(1), karl_locator);
  const karl_unittest::ServiceIsPrime is_prime(FLAGS_karl_is_prime_test_port, karl_locator);
  const karl_unittest::ServiceAnnotator annotator(
      FLAGS_karl_annotator_test_port,
      Printf("http://localhost:%d/numbers", FLAGS_karl_generator_test_port),
      Printf("http://localhost:%d/is_prime", FLAGS_karl_is_prime_test_port),
      karl_locator);
  const karl_unittest::ServiceFilter filter(
      FLAGS_karl_filter_test_port,
      Printf("http://localhost:%d/annotated", FLAGS_karl_annotator_test_port),
      karl_locator);

  ASSERT_NO_THROW(
      // 10-th (index=9 for 0-based) prime is `29`.
      const auto x29 = ParseJSON<karl_unittest::Number>(
          current::strings::Split(
              HTTP(GET(Printf("http://localhost:%d/primes?i=9&n=1", FLAGS_karl_filter_test_port))).body, '\t')
              .back());
      EXPECT_EQ(29, x29.x);
      ASSERT_TRUE(Exists(x29.is_prime));
      EXPECT_TRUE(Value(x29.is_prime)));
  ASSERT_NO_THROW(
      // 20-th (index=19 for 0-based) prime is `71`.
      const auto x71 = ParseJSON<karl_unittest::Number>(
          current::strings::Split(
              HTTP(GET(Printf("http://localhost:%d/primes?i=19&n=1", FLAGS_karl_filter_test_port))).body, '\t')
              .back());
      EXPECT_EQ(71, x71.x);
      ASSERT_TRUE(Exists(x71.is_prime));
      EXPECT_TRUE(Value(x71.is_prime)););

  {
    current::karl::ClaireStatusBase status;
    ASSERT_NO_THROW(status = ParseJSON<current::karl::ClaireStatusBase>(
                        HTTP(GET(Printf("http://localhost:%d/.current", FLAGS_karl_filter_test_port))).body));
    EXPECT_TRUE(status.registered);
    EXPECT_EQ("filter", status.service);
  }

  {
    current::karl::KarlStatus status;
    ASSERT_NO_THROW(status = ParseJSON<current::karl::KarlStatus>(
                        HTTP(GET(Printf("http://localhost:%d", FLAGS_karl_test_port))).body));
    EXPECT_EQ(4u, status.services.size());
    EXPECT_EQ("annotator", status.services[0].service);
    EXPECT_EQ("filter", status.services[1].service);
    EXPECT_EQ("generator", status.services[2].service);
    EXPECT_EQ("is_prime", status.services[3].service);
  }
}

// To run a `curl`-able test: ./.current/test --karl_run_test_forever --gtest_filter=Karl.EndToEndTest
TEST(Karl, EndToEndTest) {
  if (FLAGS_karl_run_test_forever) {
    // Instructions:
    // * Generator, Annotator, Filter: Exposed as Sherlock streams; curl `?size`, `?i=$INDEX&n=$COUNT`.
    // * IsPrime: Exposing a single endpoint; curl `?x=42` or `?x=43` to test.
    // * Karl: Displaying status; curl ``.
    // TODO(dkorolev): Have Karl expose the DOT/SVG, over the desired period of time. And test it.
    std::cerr << "Karl      :: localhost:" << FLAGS_karl_test_port << '\n';
    std::cerr << "Generator :: localhost:" << FLAGS_karl_generator_test_port << "/numbers\n";
    std::cerr << "IsPrime   :: localhost:" << FLAGS_karl_is_prime_test_port << "/is_prime?x=42\n";
    std::cerr << "Annotator :: localhost:" << FLAGS_karl_annotator_test_port << "/annotated\n";
    std::cerr << "Filter    :: localhost:" << FLAGS_karl_filter_test_port << "/primes\n";
  }

  const current::karl::Karl karl(FLAGS_karl_test_port);
  const current::karl::Locator karl_locator(Printf("http://localhost:%d", FLAGS_karl_test_port));
  const karl_unittest::ServiceGenerator generator(FLAGS_karl_generator_test_port,
                                                  std::chrono::microseconds(10000),  // 100 per second.
                                                  karl_locator);
  const karl_unittest::ServiceIsPrime is_prime(FLAGS_karl_is_prime_test_port, karl_locator);
  const karl_unittest::ServiceAnnotator annotator(
      FLAGS_karl_annotator_test_port,
      Printf("http://localhost:%d/numbers", FLAGS_karl_generator_test_port),
      Printf("http://localhost:%d/is_prime", FLAGS_karl_is_prime_test_port),
      karl_locator);
  const karl_unittest::ServiceFilter filter(
      FLAGS_karl_filter_test_port,
      Printf("http://localhost:%d/annotated", FLAGS_karl_annotator_test_port),
      karl_locator);

  {
    current::karl::ClaireStatusBase status;
    ASSERT_NO_THROW(status = ParseJSON<current::karl::ClaireStatusBase>(
                        HTTP(GET(Printf("http://localhost:%d/.current", FLAGS_karl_filter_test_port))).body));
    EXPECT_TRUE(status.registered);
    EXPECT_EQ("filter", status.service);
  }

  {
    current::karl::KarlStatus status;
    ASSERT_NO_THROW(status = ParseJSON<current::karl::KarlStatus>(
                        HTTP(GET(Printf("http://localhost:%d", FLAGS_karl_test_port))).body));
    EXPECT_EQ(4u, status.services.size());
    EXPECT_EQ("annotator", status.services[0].service);
    EXPECT_EQ("filter", status.services[1].service);
    EXPECT_EQ("generator", status.services[2].service);
    EXPECT_EQ("is_prime", status.services[3].service);
  }

  if (FLAGS_karl_run_test_forever) {
    std::cerr << "Running forever, CTRL+C to terminate.\n";
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}
