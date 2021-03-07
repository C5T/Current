/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef CURRENT_MIDICHLORIANS_CLIENT_TEST_CC
#define CURRENT_MIDICHLORIANS_CLIENT_TEST_CC

// Use the define below to enable debug output via `NSLog`.
// #define CURRENT_APPLE_ENABLE_NSLOG

#include "../../port.h"

#include <mutex>

#include "../../bricks/dflags/dflags.h"
#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

#ifdef CURRENT_APPLE

#define CURRENT_MOCK_TIME  // `SetNow()`.

#ifndef CURRENT_MIDICHLORIANS_CLIENT_IOS_IMPL_H
#include "ios/midichlorians.mm"
#include "ios/midichlorians_impl.mm"
#endif  // CURRENT_MIDICHLORIANS_CLIENT_IOS_IMPL_H

#include "../../blocks/http/api.h"

#include "../../bricks/strings/join.h"
#include "../../bricks/template/rtti_dynamic_call.h"

DEFINE_string(midichlorians_client_test_http_route, "/log", "HTTP route of the server.");

class Server {
 public:
  using events_variant_t = Variant<ios_events_t>;

  Server(current::net::ReservedPort reserved_port, const std::string& http_route)
      : http_server_(std::move(reserved_port)),
        routes_(http_server_.Register(http_route,
                                       [this](Request r) {
                                         events_variant_t event;
                                         try {
                                           event = ParseJSON<events_variant_t>(r.body);
                                           std::lock_guard<std::mutex> lock(mutex_);
                                           event.Call(*this);
                                         } catch (const current::Exception&) {
                                         }
                                       })) {}

  void operator()(const iOSAppLaunchEvent& event) {
    EXPECT_FALSE(event.device_id.empty());
    EXPECT_FALSE(event.binary_version.empty());
    // Confirm the date is past some 2001. Looks like it's the setup date of the machine. -- D.K.
    // The original test was comparing it to some late 2014, while Travis' CI instances date back to 2013.
    EXPECT_GT(event.app_install_time, 1000000000000u);
    EXPECT_GT(event.app_update_time, 1000000000000u);
    messages_.push_back(current::ToString(event.user_ms.count()) + ":Launch");
  }

  void operator()(const iOSIdentifyEvent& event) {
    messages_.push_back(current::ToString(event.user_ms.count()) + ":Identify[" + event.client_id + ']');
  }

  void operator()(const iOSFocusEvent& event) {
    if (event.gained_focus) {
      messages_.push_back(current::ToString(event.user_ms.count()) + ":GainedFocus[" + event.source + ']');
    } else {
      messages_.push_back(current::ToString(event.user_ms.count()) + ":LostFocus[" + event.source + ']');
    }
  }

  void operator()(const iOSGenericEvent& event) {
    std::string params = "source=" + event.source;
    for (const auto& f : event.fields) {
      params += ',';
      params += f.first + '=' + f.second;
    }
    messages_.push_back(current::ToString(event.user_ms.count()) + ':' + event.event + '[' + params + ']');
  }

  void operator()(const iOSBaseEvent&) {}

  size_t MessagesProcessed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return messages_.size();
  }

  std::vector<std::string> Messages() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return messages_;
  }

 private:
  mutable std::mutex mutex_;
  std::vector<std::string> messages_;
  current::http::HTTPServerPOSIX& http_server_;
  HTTPRoutesScope routes_;
};

TEST(midichloriansClient, iOSSmokeTest) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;

  Server server(std::move(reserved_port), FLAGS_midichlorians_client_test_http_route);

  current::time::ResetToZero();
  NSDictionary* launchOptions = [NSDictionary new];
  [midichlorians setup:[NSString stringWithFormat:@"http://localhost:%d%s",
                                                  port,
                                                  FLAGS_midichlorians_client_test_http_route.c_str()]
      withLaunchOptions:launchOptions];

  current::time::SetNow(std::chrono::microseconds(1000));
  [midichlorians focusEvent:YES source:@"applicationDidBecomeActive"];

  current::time::SetNow(std::chrono::microseconds(2000));
  [midichlorians identify:@"unit_test"];

  current::time::SetNow(std::chrono::microseconds(5000));
  NSDictionary* eventParams = @{ @"s" : @"str", @"b" : @true, @"x" : @1 };
  [midichlorians trackEvent:@"CustomEvent1" source:@"SmokeTest" properties:eventParams];

  current::time::SetNow(std::chrono::microseconds(15000));
  [midichlorians focusEvent:NO source:@"applicationDidEnterBackground"];

  while (server.MessagesProcessed() < 5u) {
    std::this_thread::yield();
  }

  EXPECT_EQ(5u, server.Messages().size());
  EXPECT_EQ(
      "0:Launch,"
      "1:GainedFocus[applicationDidBecomeActive],"
      "2:Identify[unit_test],"
      "5:CustomEvent1[source=SmokeTest,b=1,s=str,x=1],"
      "15:LostFocus[applicationDidEnterBackground]",
      current::strings::Join(server.Messages(), ','));
}

#endif  // CURRENT_APPLE

#endif  // CURRENT_MIDICHLORIANS_CLIENT_TEST_CC
