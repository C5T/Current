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

#ifndef SERVER_H
#define SERVER_H

#include "../../../current.h"

CURRENT_STRUCT(AddResult) {
  CURRENT_FIELD(sum, int64_t);
  CURRENT_CONSTRUCTOR(AddResult)(int64_t sum = 0) : sum(sum) {}
};

class BenchmarkTestServer {
 public:
  BenchmarkTestServer(int port, const std::string& route)
      : port_(port), scope_(HTTP(port).Register(route, [](Request r) {
          r(AddResult(current::FromString<int64_t>(r.url.query["a"]) + current::FromString<int64_t>(r.url.query["b"])));
        }) + HTTP(port).Register("/perftest", [](Request r) { r("perftest ok\n"); })) {}

  void Join() { HTTP(port_).Join(); }

 private:
  const int port_;
  HTTPRoutesScope scope_;
};

#endif  // SERVER_H
