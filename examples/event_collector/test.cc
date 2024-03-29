/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#include "../../port.h"

#include <string>
#include <sstream>
#include <thread>

#define CURRENT_MOCK_TIME  // `SetNow()`.

#include "event_collector.h"

#include "../../bricks/strings/printf.h"

#include "../../bricks/dflags/dflags.h"
#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

using current::strings::Printf;

struct EventReceiver {
  EventReceiver() : count(0u), last_t(0ull) {}
  void OnEvent(const LogEntryWithHeaders& e) {
    ++count;
    last_t = e.t;
  }

  std::atomic_size_t count;
  std::atomic<uint64_t> last_t;
};

TEST(EventCollector, Smoke) {
  current::time::ResetToZero();

  std::ostringstream os;
  EventReceiver er;

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));
  static_cast<void>(http_server);

  EventCollectorHTTPServer collector(port,
                                     os,
                                     std::chrono::milliseconds(100),
                                     "/log",
                                     "OK\n",
                                     std::bind(&EventReceiver::OnEvent, &er, std::placeholders::_1));

  current::time::SetNow(std::chrono::microseconds(12000));
  const auto get_response = HTTP(GET(Printf("http://localhost:%d/log", port)));
  EXPECT_EQ(200, static_cast<int>(get_response.code));
  EXPECT_EQ("OK\n", get_response.body);

  current::time::SetNow(std::chrono::microseconds(112000));
  while (collector.EventsPushed() < 2u) {
    std::this_thread::yield();
  }

  current::time::SetNow(std::chrono::microseconds(178000));
  const auto post_response = HTTP(POST(Printf("http://localhost:%d/log", port), "meh"));
  EXPECT_EQ(200, static_cast<int>(post_response.code));
  EXPECT_EQ("OK\n", post_response.body);

  current::time::SetNow(std::chrono::microseconds(278000));
  while (collector.EventsPushed() < 4u) {
    std::this_thread::yield();
  }

  // Strip headers from test output since they are platform dependent.
  // Using dumb way since regex-es are broken in GCC 4.8.
  std::string log = os.str();
  size_t first = 0u;
  while ((first = log.find("\"h\":{")) != std::string::npos) {
    size_t last = log.find("},", first);
    log.replace(first, last - first + 2u, "");
  }

  EXPECT_EQ(
      "{\"t\":12000,\"m\":\"GET\",\"u\":\"/log\",\"q\":{},\"c\":\"\",\"b\":\"\",\"f\":\"\"}\n"
      "{\"t\":112000,\"m\":\"TICK\",\"u\":\"\",\"q\":{},\"c\":\"\",\"b\":\"\",\"f\":\"\"}\n"
      "{\"t\":178000,\"m\":\"POST\",\"u\":\"/log\",\"q\":{},\"c\":\"\",\"b\":\"meh\",\"f\":\"\"}\n"
      "{\"t\":278000,\"m\":\"TICK\",\"u\":\"\",\"q\":{},\"c\":\"\",\"b\":\"\",\"f\":\"\"}\n",
      log);
  EXPECT_EQ(4u, er.count);
  EXPECT_EQ(278000u, er.last_t);
}

TEST(EventCollector, QueryParameters) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));
  static_cast<void>(http_server);

  std::ostringstream os;
  EventCollectorHTTPServer collector(port, os, std::chrono::microseconds(0), "/foo", "+");
  EXPECT_EQ("+", HTTP(GET(Printf("http://localhost:%d/foo?k=v&answer=42", port))).body);
  auto e = ParseJSON<LogEntryWithHeaders>(os.str());
  EXPECT_EQ(2u, e.q.size());
  EXPECT_EQ("v", e.q["k"]);
  EXPECT_EQ("42", e.q["answer"]);
}

TEST(EventCollector, Body) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));
  static_cast<void>(http_server);

  std::ostringstream os;
  EventCollectorHTTPServer collector(port, os, std::chrono::microseconds(0), "/bar", "y");
  EXPECT_EQ("y", HTTP(POST(Printf("http://localhost:%d/bar", port), "Yay!")).body);
  EXPECT_EQ("Yay!", ParseJSON<LogEntryWithHeaders>(os.str()).b);
}

TEST(EventCollector, Headers) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));
  static_cast<void>(http_server);

  std::ostringstream os;
  EventCollectorHTTPServer collector(port, os, std::chrono::microseconds(0), "/ctfo", "=");
  EXPECT_EQ("=",
            HTTP(GET(Printf("http://localhost:%d/ctfo", port)).SetHeader("foo", "bar").SetHeader("baz", "meh")).body);
  auto e = ParseJSON<LogEntryWithHeaders>(os.str());
  EXPECT_EQ("bar", e.h["foo"]);
  EXPECT_EQ("meh", e.h["baz"]);
}
