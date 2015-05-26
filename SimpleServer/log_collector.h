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

#ifndef LOG_COLLECTOR_H
#define LOG_COLLECTOR_H

#include <map>
#include <ostream>
#include <string>
#include <thread>

#include "../../Current/Bricks/net/api/api.h"
#include "../../Current/Bricks/time/chrono.h"
#include "../../Current/Bricks/cerealize/cerealize.h"

// An entry to log. Keeps everything that is easy to dig out. :-)
struct LogEntry {
  uint64_t t;                            // Unix epoch time in milliseconds.
  std::string m;                         // HTTP method.
  std::string u;                         // URL without fragments and query parameters.
  std::map<std::string, std::string> q;  // URL query parameters.
  std::string b;                         // HTTP body.
  std::string f;                         // URL fragment.

  // TODO(dkorolev): Add HTTP headers support to Bricks `net/api/types.h` HTTP client.
  // std::map<std::string, std::string> h;  // Extra HTTP headers.

  // TODO(dkorolev): Inbound IP address.
  // TODO(dkorolev): Everything else we can/should think of.
  // TODO(dkorolev): Resolve geolocation from IP?

  template <typename A>
  void serialize(A& ar) {
    ar(CEREAL_NVP(t), CEREAL_NVP(m), CEREAL_NVP(u), CEREAL_NVP(q), CEREAL_NVP(b), CEREAL_NVP(f));
  }
};

class LogCollectorHTTPServer {
 public:
  LogCollectorHTTPServer(int http_port,
                         std::ostream& ostream,
                         const std::string& route = "/log",
                         const std::string& response_text = "OK\n",
                         const size_t tick_interval_ms = 1000)  // `0` to disable tick events.
      : http_port_(http_port),
        ostream_(ostream),
        route_(route),
        response_text_(response_text),
        tick_interval_ms_(tick_interval_ms),
        send_ticks_(tick_interval_ms > 0),
        timer_thread_(&LogCollectorHTTPServer::TimerThreadFunction, this) {
    HTTP(http_port_)
        .Register(route_, [this](Request r) {
          LogEntry entry;
          entry.t = static_cast<uint64_t>(bricks::time::Now());
          entry.m = r.method;
          entry.u = r.url.url_without_parameters;
          entry.q = r.url.AllQueryParameters();
          entry.b = r.body;
          entry.f = r.url.fragment;
          // TODO(dkorolev): HTTP headers come here.
          // entry.h = { ... }
          {
            std::lock_guard<std::mutex> lock(mutex_);
            ostream_ << JSON(entry, "log_entry") << std::endl;
            last_event_t_ = entry.t;
          }
          r(response_text_);
        });
  }

  ~LogCollectorHTTPServer() {
    HTTP(http_port_).UnRegister(route_);
    send_ticks_ = false;
    timer_thread_.join();
  }

  void Join() { HTTP(http_port_).Join(); }

  void TimerThreadFunction() {
    while (send_ticks_) {
      const uint64_t now = static_cast<uint64_t>(bricks::time::Now());
      const uint64_t dt = now - last_event_t_;
      if (dt >= tick_interval_ms_) {
        std::lock_guard<std::mutex> lock(mutex_);
        LogEntry entry;
        entry.t = now;
        ostream_ << JSON(entry, "log_entry") << std::endl;
        last_event_t_ = entry.t;
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(tick_interval_ms_ - dt));
      }
    }
  }

 private:
  std::mutex mutex_;
  const int http_port_;
  std::ostream& ostream_;
  const std::string route_;
  const std::string response_text_;

  const uint64_t tick_interval_ms_;
  std::atomic_bool send_ticks_;
  uint64_t last_event_t_;
  std::thread timer_thread_;
};

#endif  // LOG_COLLECTOR_H
