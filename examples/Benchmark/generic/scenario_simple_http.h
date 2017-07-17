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

#ifndef BENCHMARK_SCENARIO_SIMPLE_HTTP_H
#define BENCHMARK_SCENARIO_SIMPLE_HTTP_H

#include "benchmark.h"

#include "../../../blocks/HTTP/api.h"

#include "../../../bricks/dflags/dflags.h"

#ifndef CURRENT_MAKE_CHECK_MODE
DEFINE_uint16(simple_http_local_port, 9700, "Local port range for `current_http_server` to use.");
DEFINE_uint16(simple_http_local_top_port,
              9749,
              "If nonzero, use ports from `--simple_http_local_port` up to this one.");
DEFINE_string(simple_http_local_route, "/perftest", "Local route for `current_http_server` to use.");
DEFINE_string(simple_http_test_body,
              "+current -nginx\n",
              "Golden HTTP body to return for the `current_http_server` scenario.");
#else
DECLARE_uint16(simple_http_local_port);
DECLARE_uint16(simple_http_local_top_port);
DECLARE_string(simple_http_local_route);
DECLARE_string(simple_http_test_body);
#endif

SCENARIO(current_http_server, "Use Current's HTTP stack for simple HTTP client-server handshake.") {
  std::vector<std::string> urls;
  HTTPRoutesScope scope;

  current_http_server() {
    const auto handler = [](Request r) { r(FLAGS_simple_http_test_body); };
    for (uint16_t port = FLAGS_simple_http_local_port;
         port <= std::max(FLAGS_simple_http_local_top_port, FLAGS_simple_http_local_port);
         ++port) {
      scope += HTTP(port).Register(FLAGS_simple_http_local_route, handler);
      urls.push_back("localhost:" + current::strings::ToString(port) + FLAGS_simple_http_local_route);
    }
  }

  void RunOneQuery() override {
    CURRENT_ASSERT(HTTP(GET(urls[rand() % urls.size()])).body == FLAGS_simple_http_test_body);
  }
};

REGISTER_SCENARIO(current_http_server);

#endif  // BENCHMARK_SCENARIO_SIMPLE_HTTP_H
