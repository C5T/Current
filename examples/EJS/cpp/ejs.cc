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

#include "../../../port.h"

#include <iostream>

#include "../../../Bricks/dflags/dflags.h"
#include "../../../Blocks/HTTP/api.h"
#include "../../../TypeSystem/struct.h"
#include "../../../TypeSystem/Serialization/json.h"

DEFINE_uint16(ejs_server_port, 3000, "The local port to spawn the current server on.");
DEFINE_uint16(ejs_renderer_port, 3001, "The local port on which the EJS rendered is expected to be up.");

CURRENT_STRUCT(PrimesResponse) {
  CURRENT_FIELD(a, int64_t);
  CURRENT_FIELD(b, int64_t);
  CURRENT_FIELD(x, std::vector<int64_t>);
};

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  auto& http_server = HTTP(FLAGS_ejs_server_port);

  const auto http_route_scope = http_server.Register(
      "/",
      [](Request request) {
        const PrimesResponse response = [&request]() {
          PrimesResponse response;
          response.a = current::FromString<int64_t>(request.url.query.get("a", "1"));
          response.b = current::FromString<int64_t>(request.url.query.get("b", "100"));
          for (int64_t p = response.a; p <= response.b; ++p) {
            bool is_prime = (p % 2) ? true : false;
            for (int64_t d = 3; d * d <= p && is_prime; d += 2) {
              if ((p % d) == 0) {
                is_prime = false;
              }
            }
            if (is_prime) {
              response.x.push_back(p);
            }
          }
          return response;
        }();

        const bool html = [&request]() {
          const char* kAcceptHeader = "Accept";
          if (request.headers.Has(kAcceptHeader)) {
            for (const auto& h : current::strings::Split(request.headers[kAcceptHeader].value, ',')) {
              if (current::strings::Split(h, ';').front() == "text/html") {  // Allow "text/html; charset=...", etc.
                return true;
              }
            }
          }
          return false;
        }();

        if (html) {
          request(HTTP(POST(current::strings::Printf("http://localhost:%d", static_cast<int>(FLAGS_ejs_renderer_port)),
                            response)).body,
                  HTTPResponseCode.OK,
                  current::net::constants::kDefaultHTMLContentType);
        } else {
          request(response);
        }
      });

  std::cout << "c++ app listening on port " << FLAGS_ejs_server_port << std::endl;

  http_server.Join();
}
