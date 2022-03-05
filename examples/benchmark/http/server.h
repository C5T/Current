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

#ifndef EXAMPLES_BENCHMARK_HTTP_SERVER_H
#define EXAMPLES_BENCHMARK_HTTP_SERVER_H

#include "../../../current.h"

CURRENT_STRUCT(AddResult) {
  CURRENT_FIELD(sum, int64_t);
  CURRENT_CONSTRUCTOR(AddResult)(int64_t sum = 0) : sum(sum) {}
};

class BenchmarkTestServer {
 public:
  BenchmarkTestServer(current::net::ReservedLocalPort reserved_port, const std::string& route)
      : port_(reserved_port),
        http_server_(HTTP(std::move(reserved_port))),
        scope_(http_server_.Register(route, [](Request r) {
          r(AddResult(current::FromString<int64_t>(r.url.query["a"]) + current::FromString<int64_t>(r.url.query["b"])));
        }) + http_server_.Register("/perftest", [](Request r) { r("perftest ok\n"); })) {}

  void Join() { http_server_.Join(); }

 private:
  const uint16_t port_;
  current::http::HTTPServerPOSIX& http_server_;
  HTTPRoutesScope scope_;
};

#endif  // EXAMPLES_BENCHMARK_HTTP_SERVER_H
