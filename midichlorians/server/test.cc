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

#include <mutex>

#define CURRENT_MOCK_TIME  // `SetNow()`.

#if defined(CURRENT_APPLE) && !defined(CURRENT_MIDICHLORIANS_CLIENT_IOS_IMPL_H)
#include "../client/ios/midichlorians.mm"
#include "../client/ios/midichlorians_impl.mm"
#endif

#include "server.h"

#include "../../bricks/strings/printf.h"

#include "../../bricks/dflags/dflags.h"
#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

namespace midichlorians_server_test {

using current::strings::Join;
using current::strings::Printf;
using namespace current::midichlorians::ios;
using namespace current::midichlorians::web;
using namespace current::midichlorians::server;

class GenericConsumer {
 public:
  GenericConsumer() {}

  void operator()(const TickLogEntry& e) {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.push_back(Printf("[%lld][Tick]", static_cast<long long>(e.server_us.count())));
  }

  void operator()(const UnparsableLogEntry& e) {
    std::lock_guard<std::mutex> lock(mutex_);
    errors_.push_back(Printf("[%lld][Error]", static_cast<long long>(e.server_us.count())));
  }

  void operator()(const EventLogEntry& e) {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.push_back(Printf("[%lld]", static_cast<long long>(e.server_us.count())));
    e.event.Call(*this);
  }

  size_t EventsCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_.size();
  }

  size_t ErrorsCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return errors_.size();
  }

  size_t EventsAndErrorsTotalCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_.size() + errors_.size();
  }

  std::vector<std::string> Events() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_;
  }

  std::vector<std::string> Errors() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return errors_;
  }

  // All methods below are expected to be called from a mutex-locked section.

  // iOS events.
  void operator()(const iOSAppLaunchEvent& event) {
    AppendToLastEvent(Printf("[iOSAppLaunchEvent user_ms=%lld]", static_cast<long long>(event.user_ms.count())));
  }

  void operator()(const iOSIdentifyEvent& event) {
    AppendToLastEvent(
        Printf("[iOSIdentifyEvent user_ms=%lld client_id=", static_cast<long long>(event.user_ms.count())) +
        event.client_id + ']');
  }

  void operator()(const iOSFocusEvent& event) {
    if (event.gained_focus) {
      AppendToLastEvent(
          Printf("[iOSFocusEvent user_ms=%lld gained_focus=true]", static_cast<long long>(event.user_ms.count())));
    } else {
      AppendToLastEvent(
          Printf("[iOSFocusEvent user_ms=%lld gained_focus=false]", static_cast<long long>(event.user_ms.count())));
    }
  }

  // LCOV_EXCL_START
  void operator()(const iOSGenericEvent& event) {
    std::string params = "source=" + event.source + " event=" + event.event;
    for (const auto& f : event.fields) {
      params += ' ';
      params += f.first + '=' + f.second;
    }
    AppendToLastEvent(Printf("[iOSGenericEvent user_ms=%lld ", static_cast<long long>(event.user_ms.count())) + params +
                      ']');
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

  void operator()(const WebEnterEvent& e) { AppendToLastEvent("[WebEnterEvent " + ExtractWebBaseEventParams(e) + ']'); }

  void operator()(const WebExitEvent& e) { AppendToLastEvent("[WebExitEvent " + ExtractWebBaseEventParams(e) + ']'); }

  void operator()(const WebGenericEvent& e) {
    AppendToLastEvent("[WebGenericEvent " + e.event_action + ':' + e.event_category + ' ' +
                      ExtractWebBaseEventParams(e) + ']');
  }

  void AppendToLastEvent(const std::string& what) {
    CURRENT_ASSERT(events_.size());
    std::string& last = events_.back();
    last += what;
  }

 private:
  mutable std::mutex mutex_;
  std::vector<std::string> events_;
  std::vector<std::string> errors_;
};

}  // namespace midichlorians_server_test

// TODO(mzhurovich): add separate event tests after midichlorians clients refactoring.

TEST(midichloriansServer, iOSEventsFromCPPSmokeTest) {
  using namespace midichlorians_server_test;

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));
  static_cast<void>(http_server);

  current::time::ResetToZero();
  GenericConsumer consumer;
  midichloriansHTTPServer<GenericConsumer> server(port, consumer, std::chrono::milliseconds(100), "/log", "OK\n");
  const std::string server_url = Printf("http://localhost:%d/log", port);

  current::time::SetNow(std::chrono::microseconds(12000));
  iOSAppLaunchEvent launch_event;
  launch_event.user_ms = std::chrono::milliseconds(1);
  ios_variant_t event1(std::move(launch_event));
  const auto response1 = HTTP(POST(server_url, JSON(event1) + "\nblah"));
  EXPECT_EQ(200, static_cast<int>(response1.code));
  EXPECT_EQ("OK\n", response1.body);

  current::time::SetNow(std::chrono::microseconds(112000));
  // Waiting for 3 entries: 1 valid, 1 unparsable and 1 tick.
  while (consumer.EventsAndErrorsTotalCount() < 3u) {
    std::this_thread::yield();
  }
  EXPECT_EQ(2u, consumer.EventsCount());
  EXPECT_EQ(1u, consumer.ErrorsCount());

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
  while (consumer.EventsAndErrorsTotalCount() < 7u) {
    std::this_thread::yield();
  }

  EXPECT_EQ(5u, consumer.EventsCount());
  EXPECT_EQ(2u, consumer.ErrorsCount());
  EXPECT_EQ(
      "[12000][iOSAppLaunchEvent user_ms=1],"
      "[112000][Tick],"
      "[203000][iOSIdentifyEvent user_ms=42 client_id=unit_test],"
      "[203000][iOSFocusEvent user_ms=50 gained_focus=true],"
      "[450000][Tick]",
      Join(consumer.Events(), ','));
  EXPECT_EQ("[12000][Error],[203000][Error]", Join(consumer.Errors(), ','));
}

#if defined(CURRENT_APPLE) && !defined(CURRENT_CI_TRAVIS)
TEST(midichloriansServer, iOSEventsFromNativeClientSmokeTest) {
  current::time::ResetToZero();

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));
  static_cast<void>(http_server);

  using namespace midichlorians_server_test;
  using namespace current::midichlorians::server;

  current::time::ResetToZero();
  GenericConsumer consumer;
  midichloriansHTTPServer<GenericConsumer> server(port, consumer, std::chrono::milliseconds(100), "/log", "OK\n");

  NSDictionary* launchOptions = [NSDictionary new];
  [midichlorians setup:[NSString stringWithFormat:@"http://localhost:%d/log", port] withLaunchOptions:launchOptions];

  current::time::SetNow(std::chrono::microseconds(1000));
  [midichlorians focusEvent:YES source:@"applicationDidBecomeActive"];

  current::time::SetNow(std::chrono::microseconds(2000));
  [midichlorians identify:@"unit_test"];

  current::time::SetNow(std::chrono::microseconds(5000));
  NSDictionary* eventParams = @{@"s" : @"str", @"b" : @true, @"x" : @1};
  [midichlorians trackEvent:@"CustomEvent1" source:@"SmokeTest" properties:eventParams];

  current::time::SetNow(std::chrono::microseconds(15000));
  [midichlorians focusEvent:NO source:@"applicationDidEnterBackground"];

  while (consumer.EventsAndErrorsTotalCount() < 5u) {
    std::this_thread::yield();
  }

  EXPECT_EQ(
      "[15000][iOSAppLaunchEvent user_ms=0],"
      "[15000][iOSFocusEvent user_ms=1 gained_focus=true],"
      "[15000][iOSIdentifyEvent user_ms=2 client_id=unit_test],"
      "[15000][iOSGenericEvent user_ms=5 source=SmokeTest event=CustomEvent1 b=1 s=str x=1],"
      "[15000][iOSFocusEvent user_ms=15 gained_focus=false]",
      Join(consumer.Events(), ','));
}
#endif  // CURRENT_APPLE && !CURRENT_CI_TRAVIS

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
  r.UserAgent("test_ua").SetHeader("X-Forwarded-For", "192.168.0.1").SetHeader("Referer", "http://myurl/page1?x=8");
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
  r.UserAgent("test_ua").SetHeader("X-Forwarded-For", "192.168.0.1").SetHeader("Referer", "http://myurl/page1?x=8");
  return r;
}

TEST(midichloriansServer, WebEventsFromCPPSmokeTest) {
  current::time::ResetToZero();

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));
  static_cast<void>(http_server);

  using namespace midichlorians_server_test;

  using namespace current::midichlorians::server;
  using namespace current::midichlorians::web;

  current::time::ResetToZero();
  GenericConsumer consumer;
  midichloriansHTTPServer<GenericConsumer> server(port, consumer, std::chrono::milliseconds(100), "/log", "OK\n");
  const std::string server_url = Printf("http://localhost:%d/log", port);

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

  while (consumer.EventsAndErrorsTotalCount() < 5u) {
    std::this_thread::yield();
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
      Join(consumer.Events(), ','));
}

#endif  // CURRENT_MIDICHLORIANS_CLIENT_SERVER_CC
