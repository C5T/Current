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

#ifndef CURRENT_MIDICHLORIANS_SERVER_TEST_CC
#define CURRENT_MIDICHLORIANS_SERVER_TEST_CC

#include "../../port.h"

#define CURRENT_MOCK_TIME  // `SetNow()`.

#if defined(CURRENT_APPLE) && !defined(CURRENT_MIDICHLORIANS_CLIENT_IOS_IMPL_H)
#include "../Client/iOS/Midichlorians.mm"
#include "../Client/iOS/MidichloriansImpl.mm"
#endif

#include "server.h"

#include "../../Bricks/strings/printf.h"

#include "../../Bricks/dflags/dflags.h"
#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_int32(midichlorians_server_test_port, PickPortForUnitTest(), "Local port to run the test.");

namespace midichlorians_server_test {

using current::strings::Join;
using current::strings::Printf;
using namespace current::midichlorians::ios;
using namespace current::midichlorians::web;
using namespace current::midichlorians::server;

struct GenericConsumer {
  GenericConsumer() : total_count(0u) {}

  void operator()(const TickLogEntry& e) {
    events.push_back(Printf("[%lld][Tick]", e.server_us.count()));
    ++total_count;
  }

  void operator()(const UnparsableLogEntry& e) {
    errors.push_back(Printf("[%lld][Error]", e.server_us.count()));
    ++total_count;
  }

  void operator()(const EventLogEntry& e) {
    events.push_back(Printf("[%lld]", e.server_us.count()));
    e.event.Call(*this);
    ++total_count;
  }

  // iOS events.
  void operator()(const iOSAppLaunchEvent& event) {
    AppendToLastEvent(Printf("[iOSAppLaunchEvent user_ms=%lld]", event.user_ms.count()));
  }

  void operator()(const iOSIdentifyEvent& event) {
    AppendToLastEvent(Printf("[iOSIdentifyEvent user_ms=%lld client_id=", event.user_ms.count()) +
                      event.client_id + ']');
  }

  void operator()(const iOSFocusEvent& event) {
    if (event.gained_focus) {
      AppendToLastEvent(Printf("[iOSFocusEvent user_ms=%lld gained_focus=true]", event.user_ms.count()));
    } else {
      AppendToLastEvent(Printf("[iOSFocusEvent user_ms=%lld gained_focus=false]", event.user_ms.count()));
    }
  }

  // LCOV_EXCL_START
  void operator()(const iOSGenericEvent& event) {
    std::string params = "source=" + event.source + " event=" + event.event;
    for (const auto& f : event.fields) {
      params += ' ';
      params += f.first + '=' + f.second;
    }
    AppendToLastEvent(Printf("[iOSGenericEvent user_ms=%lld ", event.user_ms.count()) + params + ']');
  }
  void operator()(const iOSBaseEvent&) {}
  // LCOV_EXCL_STOP

  // Web events.
  static std::string ExtractWebBaseEventParams(const WebBaseEvent& e) {
    std::string result;
    result += "customer_id=" + e.customer_id + ' ';
    result += "user_ms=" + current::ToString(e.user_ms.count()) + ' ';
    result += "client_id=" + e.client_id + ' ';
    result += "referer_host=" + e.referer_host + ' ';
    result += "referer_path=" + e.referer_path + ' ';
    bool first = true;
    for (const auto& cit : e.referer_querystring) {
      if (first) {
        first = false;
      } else {
        result += ' ';  // LCOV_EXCL_LINE
      }
      result += cit.first + '=' + cit.second;
    }
    return result;
  }

  void operator()(const WebForegroundEvent& e) {
    AppendToLastEvent("[WebForegroundEvent " + ExtractWebBaseEventParams(e) + ']');
  }

  void operator()(const WebBackgroundEvent& e) {
    AppendToLastEvent("[WebBackgroundEvent " + ExtractWebBaseEventParams(e) + ']');
  }

  void operator()(const WebEnterEvent& e) {
    AppendToLastEvent("[WebEnterEvent " + ExtractWebBaseEventParams(e) + ']');
  }

  void operator()(const WebExitEvent& e) {
    AppendToLastEvent("[WebExitEvent " + ExtractWebBaseEventParams(e) + ']');
  }

  void operator()(const WebGenericEvent& e) {
    AppendToLastEvent("[WebGenericEvent " + e.event_action + ':' + e.event_category + ' ' +
                      ExtractWebBaseEventParams(e) + ']');
  }

  void AppendToLastEvent(const std::string& what) {
    assert(events.size());
    std::string& last = events.back();
    last += what;
  }

  std::atomic_size_t total_count;
  std::vector<std::string> events;
  std::vector<std::string> errors;
};

}  // namespace midichlorians_server_test

// TODO(mzhurovich): add separate event tests after Midichlorians clients refactoring.

TEST(MidichloriansServer, iOSEventsFromCPPSmokeTest) {
  using namespace midichlorians_server_test;

  current::time::SetNow(std::chrono::microseconds(0));
  GenericConsumer consumer;
  MidichloriansHTTPServer<GenericConsumer> server(
      FLAGS_midichlorians_server_test_port, consumer, std::chrono::milliseconds(100), "/log", "OK\n");
  const std::string server_url = Printf("http://localhost:%d/log", FLAGS_midichlorians_server_test_port);

  current::time::SetNow(std::chrono::microseconds(12000));
  iOSAppLaunchEvent launch_event;
  launch_event.user_ms = std::chrono::milliseconds(1);
  ios_variant_t event1(std::move(launch_event));
  const auto response1 = HTTP(POST(server_url, JSON(event1) + "\nblah"));
  EXPECT_EQ(200, static_cast<int>(response1.code));
  EXPECT_EQ("OK\n", response1.body);

  current::time::SetNow(std::chrono::microseconds(112000));
  // Waiting for 3 entries: 1 valid, 1 unparsable and 1 tick.
  while (consumer.total_count < 3u) {
    ;  // Spin lock.
  }
  EXPECT_EQ(2u, consumer.events.size());
  EXPECT_EQ(1u, consumer.errors.size());

  current::time::SetNow(std::chrono::microseconds(203000));
  iOSIdentifyEvent identify_event;
  identify_event.user_ms = std::chrono::milliseconds(42);
  identify_event.client_id = "unit_test";
  ios_variant_t event2(std::move(identify_event));
  iOSFocusEvent focus_event;
  focus_event.user_ms = std::chrono::milliseconds(50);
  focus_event.gained_focus = true;
  ios_variant_t event3(std::move(focus_event));
  const auto response2 = HTTP(POST(server_url, JSON(event2) + "\nmeh\n" + JSON(event3)));
  EXPECT_EQ(200, static_cast<int>(response2.code));
  EXPECT_EQ("OK\n", response2.body);

  current::time::SetNow(std::chrono::microseconds(450000));
  // Waiting for 4 more entries: 2 valid, 1 unparsable and 1 tick.
  while (consumer.total_count < 7u) {
    ;  // Spin lock.
  }

  EXPECT_EQ(5u, consumer.events.size());
  EXPECT_EQ(2u, consumer.errors.size());
  EXPECT_EQ(
      "[12000][iOSAppLaunchEvent user_ms=1],"
      "[112000][Tick],"
      "[203000][iOSIdentifyEvent user_ms=42 client_id=unit_test],"
      "[203000][iOSFocusEvent user_ms=50 gained_focus=true],"
      "[450000][Tick]",
      Join(consumer.events, ','));
  EXPECT_EQ("[12000][Error],[203000][Error]", Join(consumer.errors, ','));
}

#ifdef CURRENT_APPLE
TEST(MidichloriansServer, iOSEventsFromNativeClientSmokeTest) {
  using namespace midichlorians_server_test;
  using namespace current::midichlorians::server;

  current::time::SetNow(std::chrono::microseconds(0));
  GenericConsumer consumer;
  MidichloriansHTTPServer<GenericConsumer> server(
      FLAGS_midichlorians_server_test_port, consumer, std::chrono::milliseconds(100), "/log", "OK\n");

  NSDictionary* launchOptions = [NSDictionary new];
  [Midichlorians setup:[NSString
                           stringWithFormat:@"http://localhost:%d/log", FLAGS_midichlorians_server_test_port]
      withLaunchOptions:launchOptions];

  current::time::SetNow(std::chrono::microseconds(1000));
  [Midichlorians focusEvent:YES source:@"applicationDidBecomeActive"];

  current::time::SetNow(std::chrono::microseconds(2000));
  [Midichlorians identify:@"unit_test"];

  current::time::SetNow(std::chrono::microseconds(5000));
  NSDictionary* eventParams = @{ @"s" : @"str", @"b" : @true, @"x" : @1 };
  [Midichlorians trackEvent:@"CustomEvent1" source:@"SmokeTest" properties:eventParams];

  current::time::SetNow(std::chrono::microseconds(15000));
  [Midichlorians focusEvent:NO source:@"applicationDidEnterBackground"];

  while (consumer.total_count < 5u) {
    ;  // Spin lock.
  }

  EXPECT_EQ(
      "[15000][iOSAppLaunchEvent user_ms=0],"
      "[15000][iOSFocusEvent user_ms=1 gained_focus=true],"
      "[15000][iOSIdentifyEvent user_ms=2 client_id=unit_test],"
      "[15000][iOSGenericEvent user_ms=5 source=SmokeTest event=CustomEvent1 b=1 s=str x=1],"
      "[15000][iOSFocusEvent user_ms=15 gained_focus=false]",
      Join(consumer.events, ','));
}
#endif  // CURRENT_APPLE

GET MockGETRequest(const std::string& base_url,
                   uint64_t ms,
                   const std::string& ea,
                   const std::string& ec = "default_category") {
  const std::string query_params =
      "CUSTOMER_ACCOUNT=test_customer"
      "&cid=unit_test"
      "&_t=" +
      current::ToString(ms) + "&ea=" + ea + "&ec=" + ec;
  GET r(base_url + '?' + query_params);
  r.UserAgent("test_ua")
      .SetHeader("X-Forwarded-For", "192.168.0.1")
      .SetHeader("Referer", "http://myurl/page1?x=8");
  return r;
}

POST MockPOSTRequest(const std::string& base_url,
                     uint64_t ms,
                     const std::string& ea,
                     const std::string& ec = "default_category") {
  const std::string query_params = "CUSTOMER_ACCOUNT=test_customer";
  const std::string body =
      "&cid=unit_test"
      "&_t=" +
      current::ToString(ms) + "&ea=" + ea + "&ec=" + ec;
  POST r(base_url + '?' + query_params, body);
  r.UserAgent("test_ua")
      .SetHeader("X-Forwarded-For", "192.168.0.1")
      .SetHeader("Referer", "http://myurl/page1?x=8");
  return r;
}

TEST(MidichloriansServer, WebEventsFromCPPSmokeTest) {
  using namespace midichlorians_server_test;

  using namespace current::midichlorians::server;
  using namespace current::midichlorians::web;

  current::time::SetNow(std::chrono::microseconds(0));
  GenericConsumer consumer;
  MidichloriansHTTPServer<GenericConsumer> server(
      FLAGS_midichlorians_server_test_port, consumer, std::chrono::milliseconds(100), "/log", "OK\n");
  const std::string server_url = Printf("http://localhost:%d/log", FLAGS_midichlorians_server_test_port);

  current::time::SetNow(std::chrono::microseconds(1000));
  const auto response1 = HTTP(MockGETRequest(server_url, 1, "Fg"));
  EXPECT_EQ(200, static_cast<int>(response1.code));
  EXPECT_EQ("OK\n", response1.body);

  current::time::SetNow(std::chrono::microseconds(2000));
  HTTP(MockPOSTRequest(server_url, 2, "Bg"));
  current::time::SetNow(std::chrono::microseconds(3000));
  HTTP(MockPOSTRequest(server_url, 3, "En"));
  current::time::SetNow(std::chrono::microseconds(4000));
  HTTP(MockGETRequest(server_url, 4, "Ex"));
  current::time::SetNow(std::chrono::microseconds(10000));
  HTTP(MockGETRequest(server_url, 10, "action", "category"));

  while (consumer.total_count < 5u) {
    ;  // Spin lock.
  }

  EXPECT_EQ(
      "[1000][WebForegroundEvent customer_id=test_customer user_ms=1 client_id=unit_test referer_host=myurl "
      "referer_path=/page1 x=8],"
      "[2000][WebBackgroundEvent customer_id=test_customer user_ms=2 client_id=unit_test referer_host=myurl "
      "referer_path=/page1 x=8],"
      "[3000][WebEnterEvent customer_id=test_customer user_ms=3 client_id=unit_test referer_host=myurl "
      "referer_path=/page1 x=8],"
      "[4000][WebExitEvent customer_id=test_customer user_ms=4 client_id=unit_test referer_host=myurl "
      "referer_path=/page1 x=8],"
      "[10000][WebGenericEvent action:category customer_id=test_customer user_ms=10 client_id=unit_test "
      "referer_host=myurl referer_path=/page1 x=8]",
      Join(consumer.events, ','));
}

#endif  // CURRENT_MIDICHLORIANS_CLIENT_SERVER_CC
