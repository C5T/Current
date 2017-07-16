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

#ifndef BENCHMARK_SCENARIO_NGINX_HTTP_CLIENT_H
#define BENCHMARK_SCENARIO_NGINX_HTTP_CLIENT_H

#include "benchmark.h"

#include "../../../bricks/dflags/dflags.h"
#include "../../../bricks/strings/escape.h"

#include "../../../Blocks/HTTP/api.h"

#include "../../../Utils/Nginx/nginx.h"

#ifndef CURRENT_MAKE_CHECK_MODE
DEFINE_string(nginx_client_config_file_prefix,
              "/etc/nginx/sites-enabled/current_perftest_",
              "The nginx config to use.");
DEFINE_uint16(nginx_client_local_port, 9750, "Local port range for `simple_nginx_client` to use.");
DEFINE_uint16(nginx_client_local_top_port,
              9799,
              "If nonzero, use ports from `--nginx_client_local_port,` up to this one.");
DEFINE_string(nginx_client_local_route, "/perftest", "Local route for `simple_nginx_client` to use.");
DEFINE_string(nginx_client_test_body,
              "+nginx -current\n",
              "Golden HTTP body to return for the `simple_nginx_client` scenario.");
#else
DECLARE_string(nginx_client_config_file_prefix);
DECLARE_uint16(nginx_client_local_port);
DECLARE_uint16(nginx_client_local_top_port);
DECLARE_string(nginx_client_local_route);
DECLARE_string(nginx_client_test_body);
#endif

SCENARIO(simple_nginx_client, "Use Current's HTTP client to access an nginx-controlled HTTP endpoint.") {
  std::vector<std::string> urls;

  simple_nginx_client() {
    using namespace current::nginx::config;

    for (uint16_t port = FLAGS_nginx_client_local_port;
         port <= std::max(FLAGS_nginx_client_local_port, FLAGS_nginx_client_local_top_port);
         ++port) {
      current::nginx::NginxManager nginx(FLAGS_nginx_client_config_file_prefix + current::strings::ToString(port));

      current::nginx::config::ServerDirective server_directive(port);
      server_directive.CreateLocation(FLAGS_nginx_client_local_route)
          .Add(SimpleDirective("return",
                               "200 \"" + current::strings::EscapeForCPlusPlus(FLAGS_nginx_client_test_body) + '\"'));

      std::cerr << "nginx setup on port " << port << " ...";
      nginx.UpdateConfig(std::move(server_directive));
      std::cerr << "\b\b\bOK.\n";

      urls.push_back("localhost:" + current::strings::ToString(port) + FLAGS_nginx_client_local_route);
    }
  }

  void RunOneQuery() override {
    CURRENT_ASSERT(HTTP(GET(urls[rand() % urls.size()])).body == FLAGS_nginx_client_test_body);
  }
};

REGISTER_SCENARIO(simple_nginx_client);

#endif  // BENCHMARK_SCENARIO_NGINX_HTTP_CLIENT_H
