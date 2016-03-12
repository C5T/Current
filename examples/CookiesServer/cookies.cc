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

#include "../../current.h"

using namespace current;

DEFINE_int32(cookies_demo_port, 3000, "Local port to spawn the server on.");

CURRENT_STRUCT(CookiesTestResponse) {
  CURRENT_FIELD(cookies, (std::map<std::string, std::string>));
  CURRENT_DEFAULT_CONSTRUCTOR(CookiesTestResponse) {}
  CURRENT_CONSTRUCTOR(CookiesTestResponse)(const std::map<std::string, std::string>& c) : cookies(c) {}
};

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  HTTP(FLAGS_cookies_demo_port)
      .Register("/",
                [](Request r) {
                  const auto now = current::ToString(current::time::Now().count());
                  r(Response(CookiesTestResponse(r.headers.cookies))
                        .SetCookie("Now", now)
                        .SetCookie("LoadedAt" + now, "Yes."));
                });

  HTTP(FLAGS_cookies_demo_port).Join();
}
