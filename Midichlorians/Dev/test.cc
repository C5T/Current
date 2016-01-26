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

// Exclude the whole Midichlorians Beta branch when running the top-level `make test`.
#ifndef CURRENT_COVERAGE_REPORT_MODE

// Use the define below to enable debug output via `NSLog`.
// #define CURRENT_APPLE_ENABLE_NSLOG

#include "../../port.h"

#ifdef CURRENT_APPLE

#include "Beta/iOS/Midichlorians.mm"
#include "Beta/iOS/MidichloriansImpl.mm"

#include "../../Blocks/HTTP/api.h"

#include "../../Bricks/strings/join.h"
#include "../../Bricks/template/rtti_dynamic_call.h"

#include "../../Bricks/dflags/dflags.h"
#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_int32(midichlorians_dev_test_http_port, PickPortForUnitTest(), "Port to spawn server on.");
DEFINE_string(midichlorians_dev_test_http_route, "/log", "HTTP route of the server.");

class Server {
 public:
  using T_EVENTS = TypeList<iOSAppLaunchEvent, iOSIdentifyEvent, iOSFocusEvent, iOSGenericEvent>;

  Server(int http_port, const std::string& http_route)
      : messages_processed_(0u),
        routes_(HTTP(http_port).Register(http_route,
                                         [this](Request r) {
                                           std::unique_ptr<MidichloriansEvent> event;
                                           try {
                                             CerealizeParseJSON(r.body, event);
                                             current::metaprogramming::RTTIDynamicCall<T_EVENTS>(event, *this);
                                             ++messages_processed_;
                                           } catch (const current::Exception&) {
                                           }
                                         })) {}

  void operator()(const iOSAppLaunchEvent& event) {
    EXPECT_FALSE(event.device_id.empty());
    EXPECT_FALSE(event.binary_version.empty());
    EXPECT_GT(event.app_install_time, 1420000000000u);
    EXPECT_GT(event.app_update_time, 1420000000000u);
    EXPECT_EQ("*FinishLaunchingWithOptions", event.description);
    messages_.push_back("Launch");
  }

  void operator()(const iOSIdentifyEvent& event) { messages_.push_back("Identify[" + event.client_id + ']'); }

  void operator()(const iOSFocusEvent& event) {
    if (event.gained_focus) {
      messages_.push_back("GainedFocus[" + event.description + ']');
    } else {
      messages_.push_back("LostFocus[" + event.description + ']');
    }
  }

  void operator()(const iOSGenericEvent& event) {
    std::string params = "source=" + event.source;
    for (const auto& f : event.fields) {
      params += ',';
      params += f.first + '=' + f.second;
    }
    for (const auto& f : event.complex_fields) {
      params += ',';
      params += f.first + '=' + f.second;
    }
    messages_.push_back(event.event + '[' + params + ']');
  }

  size_t MessagesProcessed() const { return messages_processed_; }

  const std::vector<std::string>& Messages() const { return messages_; }

 private:
  std::vector<std::string> messages_;
  std::atomic_size_t messages_processed_;
  HTTPRoutesScope routes_;
};

TEST(Midichlorians, SmokeTest) {
  Server server(FLAGS_midichlorians_dev_test_http_port, FLAGS_midichlorians_dev_test_http_route);

  NSDictionary* launchOptions = [NSDictionary new];
  [Midichlorians setup:[NSString stringWithFormat:@"http://localhost:%d%s",
                                                  FLAGS_midichlorians_dev_test_http_port,
                                                  FLAGS_midichlorians_dev_test_http_route.c_str()]
      withLaunchOptions:launchOptions];

  [Midichlorians focusEvent:YES source:@"applicationDidBecomeActive"];

  [Midichlorians identify:@"unit_test"];

  NSDictionary* eventParams = @{ @"s" : @"str", @"b" : @true, @"x" : @1 };
  [Midichlorians trackEvent:@"CustomEvent1" source:@"SmokeTest" properties:eventParams];

  [Midichlorians focusEvent:NO source:@"applicationDidEnterBackground"];

  while (server.MessagesProcessed() < 5u) {
    ;  // spin lock.
  }

  EXPECT_EQ(
      "Launch,"
      "GainedFocus[applicationDidBecomeActive],"
      "Identify[unit_test],"
      "CustomEvent1[source=SmokeTest,s=str,b=1,x=1],"
      "LostFocus[applicationDidEnterBackground]",
      current::strings::Join(server.Messages(), ','));
}

#endif  // CURRENT_APPLE

#endif  // CURRENT_COVERAGE_REPORT_MODE
