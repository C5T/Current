/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#include <string>
#include <sstream>

#define BRICKS_MOCK_TIME  // `SetNow()`.

#include "log_collector.h"

#include "../Bricks/strings/printf.h"

#include "../Bricks/dflags/dflags.h"
#include "../Bricks/3party/gtest/gtest-main-with-dflags.h"

DEFINE_int32(log_collector_test_port, 8089, "Local port to run the test.");

using bricks::strings::Printf;

TEST(LogCollector, Smoke) {
  std::ostringstream os;
  LogCollectorHTTPServer collector(FLAGS_log_collector_test_port, os);

  bricks::time::SetNow(static_cast<bricks::time::EPOCH_MILLISECONDS>(1234));
  const auto get_response = HTTP(GET(Printf("http://localhost:%d/log", FLAGS_log_collector_test_port)));
  EXPECT_EQ(200, static_cast<int>(get_response.code));
  EXPECT_EQ("OK\n", get_response.body);

  bricks::time::SetNow(static_cast<bricks::time::EPOCH_MILLISECONDS>(5678));
  const auto post_response =
      HTTP(POST(Printf("http://localhost:%d/log", FLAGS_log_collector_test_port), "meh"));
  EXPECT_EQ(200, static_cast<int>(post_response.code));
  EXPECT_EQ("OK\n", post_response.body);
  EXPECT_EQ(
      "{\"log_entry\":{\"t\":1234,\"m\":\"GET\",\"u\":\"/log\",\"q\":[],\"b\":\"\",\"f\":\"\"}}\n"
      "{\"log_entry\":{\"t\":5678,\"m\":\"POST\",\"u\":\"/log\",\"q\":[],\"b\":\"meh\",\"f\":\"\"}}\n",
      os.str());
}

TEST(LogCollector, QueryParameters) {
  std::ostringstream os;
  LogCollectorHTTPServer collector(FLAGS_log_collector_test_port, os, "/foo", "+");
  EXPECT_EQ("+",
            HTTP(GET(Printf("http://localhost:%d/foo?k=v&answer=42", FLAGS_log_collector_test_port))).body);
  auto e = ParseJSON<LogEntry>(os.str());
  EXPECT_EQ(2u, e.q.size());
  EXPECT_EQ("v", e.q["k"]);
  EXPECT_EQ("42", e.q["answer"]);
}

TEST(LogCollector, Body) {
  std::ostringstream os;
  LogCollectorHTTPServer collector(FLAGS_log_collector_test_port, os, "/bar", "y");
  EXPECT_EQ("y", HTTP(POST(Printf("http://localhost:%d/bar", FLAGS_log_collector_test_port), "Yay!")).body);
  EXPECT_EQ("Yay!", ParseJSON<LogEntry>(os.str()).b);
}

TEST(LogCollector, URLFragment) {
  std::ostringstream os;
  LogCollectorHTTPServer collector(FLAGS_log_collector_test_port, os, "/baz", "Y");
  EXPECT_EQ("Y", HTTP(GET(Printf("http://localhost:%d/baz#meh", FLAGS_log_collector_test_port))).body);
  EXPECT_EQ("meh", ParseJSON<LogEntry>(os.str()).f);
}

// TODO(dkorolev): Add extra headers support into `net/api/types.h`. And test them on Mac.
#if 0
TEST(LogCollector, ExtraHeaders) {
  std::ostringstream os;
  LogCollectorHTTPServer collector(FLAGS_log_collector_test_port, os, "/ctfo", "=");
  EXPECT_EQ("=",
            HTTP(GET(Printf("http://localhost:%d/ctfo", FLAGS_log_collector_test_port))
                     .SetHeader("foo", "bar")
                     .SetHeader("baz", "meh")).body);
  auto e = ParseJSON<LogEntry>(os.str());
  EXPECT_EQ(2u, e.h.size());
  EXPECT_EQ("bar", e.h["foo"]);
  EXPECT_EQ("meh", e.h["baz"]);
}
#endif
