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

#include "sherlock.h"

#include "../Bricks/time/chrono.h"
#include "../Bricks/cerealize/cerealize.h"
#include "../Bricks/net/api/api.h"
#include "../Bricks/dflags/dflags.h"

DEFINE_int32(port, 8191, "Local port to use.");

struct Point {
  double x;
  double y;
  template <typename A>
  void serialize(A& ar) {
    ar(CEREAL_NVP(x), CEREAL_NVP(y));
  }
};

// TODO(dkorolev): This class should be moved into `sherlock.h`.
template <typename T>
class ServeJSONOverHTTP {
 public:
  ServeJSONOverHTTP(Request&& r)
      : http_request_scope_(std::move(r)), http_response_(http_request_scope_.SendChunkedResponse()) {}

  inline bool Entry(const T& entry) {
    try {
      http_response_(JSON(entry, "point") + "\n");  // TODO(dkorolev): WTF do I have to say JSON() here?
      return true;
    } catch (const bricks::net::NetworkException&) {
      return false;
    }
  }

  inline bool TerminationRequest() { return true; }

 private:
  Request http_request_scope_;  // Need to keep `Request` in scope, for the lifetime of the chunked response.
  bricks::net::HTTPServerConnection::ChunkedResponseSender http_response_;

  ServeJSONOverHTTP() = delete;
  ServeJSONOverHTTP(const ServeJSONOverHTTP&) = delete;
  void operator=(const ServeJSONOverHTTP&) = delete;
  ServeJSONOverHTTP(ServeJSONOverHTTP&&) = delete;
  void operator=(ServeJSONOverHTTP&&) = delete;
};

int main() {
  auto time_series = sherlock::Stream<Point>("time_series");
  std::thread delayed_publishing_thread([&time_series]() {
    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
      double x = static_cast<double>(bricks::time::Now());
      time_series.Publish(Point{x, 0.5 * (1.0 + sin(0.0025 * x))});
    }
  });
  HTTP(FLAGS_port).Register("/time_series", [&time_series](Request r) {
    // Spawn a new thread since we have to, for the sake of this demo. Yes, it's a hack -- D.K.
    // TODO(dkorolev): This is to go away as soon as I resolve the handler ownership issue.
    std::thread([&time_series](Request r) {
      // TODO(dkorolev): Figure out how to best handle ownership of the parameter wrt destruction.
      // For now it's a hack.
      try {
        ServeJSONOverHTTP<Point> server(std::move(r));
        auto tmp = time_series.Subscribe(server);
        tmp->Join();  // TODO(dkorolev): It's not going to be `.Join()` in prod here.
      } catch (const bricks::net::NetworkException&) {
      }
    }, std::move(r)).detach();
  });
  HTTP(FLAGS_port).Join();
}
