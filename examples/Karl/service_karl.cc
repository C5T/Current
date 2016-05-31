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

#include "current_build.h"

#include "../../Karl/test_service/is_prime.h"
#include "../../Karl/karl.h"
#include "../../Bricks/dflags/dflags.h"

DEFINE_uint16(nginx_port, 7590, "Port for Nginx to serve proxied queries to Claires.");
DEFINE_string(nginx_config, "", "If set, Karl updates this config with the proxy routes to Claires.");

int main(int argc, char **argv) {
  using namespace current::karl::constants;
  ParseDFlags(&argc, &argv);
  auto params = current::karl::KarlParameters()
                    .SetKeepalivesPort(kDefaultKarlPort)
                    .SetFleetViewPort(kDefaultKarlFleetViewPort)
                    .SetStreamFile(".current/stream")    // TODO(dkorolev): Windows-friendly.
                    .SetStorageFile(".current/storage")  // TODO(dkorolev): Windows-friendly.
                    .SetSVGName("Karl's example")
                    .SetGitHubURL("https://github.com/dkorolev/Current");
  if (!FLAGS_nginx_config.empty()) {
    params.SetNginxParameters(current::karl::KarlNginxParameters(FLAGS_nginx_port, FLAGS_nginx_config));
  }
  const current::karl::GenericKarl<current::karl::UseOwnStorage,
                                   current::karl::default_user_status::status,
                                   karl_unittest::is_prime> karl(params);
  std::cout << "Karl up, http://localhost:" << kDefaultKarlFleetViewPort << '/' << std::endl;
  if (!FLAGS_nginx_config.empty()) {
    std::cout << "Karl's Nginx is serving on http://localhost:" << FLAGS_nginx_port << '/' << std::endl;
  }
  HTTP(kDefaultKarlPort).Join();
}
