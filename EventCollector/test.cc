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
#include <thread>

#define BRICKS_MOCK_TIME  // `SetNow()`.

#include "event_collector.h"

#include "../../Current/Bricks/strings/printf.h"

#include "../../Current/Bricks/dflags/dflags.h"
#include "../../Current/Bricks/3party/gtest/gtest-main-with-dflags.h"

DEFINE_int32(event_collector_test_port, 8089, "Local port to run the test.");

using bricks::strings::Printf;

struct EventReceiver {
  EventReceiver() : count(0u) {}
  void OnEvent(const LogEntry& e) {
    ++count;
    last_t = e.t;
  }

  std::atomic_size_t count;
  std::atomic<uint64_t> last_t;
};

TEST(EventCollector, Smoke) {
  std::ostringstream os;
  EventReceiver er;

  bricks::time::SetNow(static_cast<bricks::time::EPOCH_MILLISECONDS>(0));
  EventCollectorHTTPServer collector(FLAGS_event_collector_test_port,
                                     os,
                                     static_cast<bricks::time::MILLISECONDS_INTERVAL>(100),
                                     "/log",
                                     "OK\n",
                                     std::bind(&EventReceiver::OnEvent, &er, std::placeholders::_1));

  bricks::time::SetNow(static_cast<bricks::time::EPOCH_MILLISECONDS>(12));
  const auto get_response = HTTP(GET(Printf("http://localhost:%d/log", FLAGS_event_collector_test_port)));
  EXPECT_EQ(200, static_cast<int>(get_response.code));
  EXPECT_EQ("OK\n", get_response.body);

  bricks::time::SetNow(static_cast<bricks::time::EPOCH_MILLISECONDS>(112));
  while (collector.EventsPushed() < 2u) {
    ;  // Spin lock.
  }

  bricks::time::SetNow(static_cast<bricks::time::EPOCH_MILLISECONDS>(178));
  const auto post_response =
      HTTP(POST(Printf("http://localhost:%d/log", FLAGS_event_collector_test_port), "meh"));
  EXPECT_EQ(200, static_cast<int>(post_response.code));
  EXPECT_EQ("OK\n", post_response.body);

  bricks::time::SetNow(static_cast<bricks::time::EPOCH_MILLISECONDS>(278));
  while (collector.EventsPushed() < 4u) {
    ;  // Spin lock.
  }

  EXPECT_EQ(
      "{\"log_entry\":{\"t\":12,\"m\":\"GET\",\"u\":\"/log\",\"q\":[],\"b\":\"\",\"f\":\"\"}}\n"
      "{\"log_entry\":{\"t\":112,\"m\":\"TICK\",\"u\":\"\",\"q\":[],\"b\":\"\",\"f\":\"\"}}\n"
      "{\"log_entry\":{\"t\":178,\"m\":\"POST\",\"u\":\"/log\",\"q\":[],\"b\":\"meh\",\"f\":\"\"}}\n"
      "{\"log_entry\":{\"t\":278,\"m\":\"TICK\",\"u\":\"\",\"q\":[],\"b\":\"\",\"f\":\"\"}}\n",
      os.str());
  EXPECT_EQ(4u, er.count);
  EXPECT_EQ(278u, er.last_t);
}

TEST(EventCollector, QueryParameters) {
  std::ostringstream os;
  EventCollectorHTTPServer collector(FLAGS_event_collector_test_port, os,
                                   static_cast<bricks::time::MILLISECONDS_INTERVAL>(0), "/foo", "+");
  EXPECT_EQ("+",
            HTTP(GET(Printf("http://localhost:%d/foo?k=v&answer=42", FLAGS_event_collector_test_port))).body);
  auto e = ParseJSON<LogEntry>(os.str());
  EXPECT_EQ(2u, e.q.size());
  EXPECT_EQ("v", e.q["k"]);
  EXPECT_EQ("42", e.q["answer"]);
}

TEST(EventCollector, Body) {
  std::ostringstream os;
  EventCollectorHTTPServer collector(FLAGS_event_collector_test_port, os,
                                   static_cast<bricks::time::MILLISECONDS_INTERVAL>(0), "/bar", "y");
  EXPECT_EQ("y", HTTP(POST(Printf("http://localhost:%d/bar", FLAGS_event_collector_test_port), "Yay!")).body);
  EXPECT_EQ("Yay!", ParseJSON<LogEntry>(os.str()).b);
}

// TODO(dkorolev): Add extra headers support into `net/api/types.h`. And test them on Mac.
#if 0
TEST(EventCollector, ExtraHeaders) {
  std::ostringstream os;
  EventCollectorHTTPServer collector(FLAGS_event_collector_test_port, os, "/ctfo", "=");
  EXPECT_EQ("=",
            HTTP(GET(Printf("http://localhost:%d/ctfo", FLAGS_event_collector_test_port))
                     .SetHeader("foo", "bar")
                     .SetHeader("baz", "meh")).body);
  auto e = ParseJSON<LogEntry>(os.str());
  EXPECT_EQ(2u, e.h.size());
  EXPECT_EQ("bar", e.h["foo"]);
  EXPECT_EQ("meh", e.h["baz"]);
}
#endif
