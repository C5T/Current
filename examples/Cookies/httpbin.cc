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

// A real-life test for `Cookie` setting.

/*
# nginx
location /httpbinmagic {
  proxy_pass https://httpbin.org/cookies; 
  proxy_pass_request_headers on;
}

# test
$ curl localhost/httpbinmagic
{
  "cookies": {}
}
$ curl -H "Cookie: foo=bar" localhost/httpbinmagic
{
  "cookies": {
    "foo": "bar"
  }
}
*/

#include "../../current.h"

using namespace current;

int main() {
  std::cerr << HTTP(GET("localhost/httpbinmagic")).body << std::endl;
  std::cerr << HTTP(GET("localhost/httpbinmagic").SetCookie("foo", "bar")).body << std::endl;
}
