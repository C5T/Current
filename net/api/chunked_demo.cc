/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

/*

# This binary sends back JSON-s, broken into chunks in random ways.
# GET (or, actually, use any method) "/" for an infinite stream of data.
# Use path "/?t=<seconds>" for a finite number of seconds, "http://localhost:8181/?t=1" works in the browser.

# Here is how it looks like for curl.
$ curl -s localhost:8181 | head -n 20
{x:0.045000,y:0.044985}
{x:0.085000,y:0.084898}
{x:0.120000,y:0.119712}
{x:0.145000,y:0.144492}
{x:0.175000,y:0.174108}
{x:0.210000,y:0.208460}
{x:0.240000,y:0.237703}
{x:0.255000,y:0.252245}
{x:0.285000,y:0.281157}
{x:0.320000,y:0.314567}
{x:0.335000,y:0.328769}
{x:0.335000,y:0.328769}
{x:0.380000,y:0.370920}
{x:0.410000,y:0.398609}
{x:0.445000,y:0.430458}
{x:0.465000,y:0.448423}
{x:0.480000,y:0.461779}
{x:0.525000,y:0.501213}
{x:0.540000,y:0.514136}
{x:0.545000,y:0.518418}

# And here is how it looks like for nc.
$ echo -e "GET /\r\n\r\n" | nc localhost 8181 | head -n 40
HTTP/1.1 200 OK
Content-Type: text/plain
Transfer-Encoding: chunked

1
{
2
x:
11
0.040000,y:0.0399
12
89}
{x:0.075000,y:
E
0.074930}
{x:0
32
.095000,y:0.094857}
{x:0.135000,y:0.134590}
{x:0.1
5
85000
1
,
1E
y:0.183947}
{x:0.215000,y:0.21
16
3347}
{x:0.215000,y:0.
2A
213347}
{x:0.220000,y:0.218230}
{x:0.25000
3
0,y
3
:0.
26
247404}

*/

#include <string>

#include "api.h"

#include "../../dflags/dflags.h"
#include "../../strings/printf.h"
#include "../../time/chrono.h"

using namespace bricks::net::api;
using bricks::time::Now;
using bricks::strings::Printf;

DEFINE_int32(port, 8181, "The port to serve chunked response on.");

// TODO(dkorolev): Add multithreading. This implies two features:
// 1) Allow std::move<Request> to actually not close the connection from the original one, and
// 2) Register the client so that the server can be gracefully terminated.
// Right now, the `std::thread()` part is just commented out.
int main() {
  HTTP(FLAGS_port).Register("/", [](Request&& r) {
    //    std::thread([](Request&& r) {
    auto response = r.connection.SendChunkedHTTPResponse();
    std::string data;
    const double begin = static_cast<double>(Now());
    const double t = atof(r.url.query["t"].c_str());
    const double end = (t > 0) ? (begin + t * 1e3) : 1e18;
    double current;
    while ((current = static_cast<double>(Now())) < end) {
      std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 10));
      const double x = 5e-3 * (current - begin);
      const double y = sin(x);
      data += Printf("{x:%lf,y:%lf}\n", x, y);
      const double f = (rand() % 101) * (rand() % 101) * (rand() % 101) * 1e-6;
      const size_t n = static_cast<size_t>(data.length() * f);
      if (n) {
        response.Send(data.substr(0, n));
        data = data.substr(n);
      }
    }
    //   }, std::move(r)).detach();
  });
  HTTP(FLAGS_port).Join();
}
