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

#ifndef EVENT_COLLECTOR_H
#define EVENT_COLLECTOR_H

#include <functional>
#include <map>
#include <ostream>
#include <string>
#include <thread>

#include "../../blocks/HTTP/api.h"
#include "../../bricks/time/chrono.h"

#include "../../typesystem/struct.h"
#include "../../typesystem/serialization/json.h"

// Initial version of `LogEntry` structure.
CURRENT_STRUCT(LogEntry) {
  CURRENT_FIELD(t, uint64_t);                              // Unix epoch time in microseconds.
  CURRENT_FIELD(m, std::string);                           // HTTP method.
  CURRENT_FIELD(u, std::string);                           // URL without fragments and query parameters.
  CURRENT_FIELD(q, (std::map<std::string, std::string>));  // URL query parameters.
  CURRENT_FIELD(b, std::string);                           // HTTP body.
  CURRENT_FIELD(f, std::string);                           // URL fragment.
};

// Improved version with HTTP headers.
CURRENT_STRUCT(LogEntryWithHeaders) {
  CURRENT_FIELD(t, uint64_t);                              // Unix epoch time in microseconds.
  CURRENT_FIELD(m, std::string);                           // HTTP method.
  CURRENT_FIELD(u, std::string);                           // URL without fragments and query parameters.
  CURRENT_FIELD(q, (std::map<std::string, std::string>));  // URL query parameters.
  CURRENT_FIELD(h, (std::map<std::string, std::string>));  // HTTP headers.
  CURRENT_FIELD(c, std::string);                           // HTTP cookies, as string.
  CURRENT_FIELD(b, std::string);                           // HTTP body.
  CURRENT_FIELD(f, std::string);                           // URL fragment.
  // TODO(dkorolev): Inbound IP address.
  // TODO(dkorolev): Everything else we can/should think of.
  // TODO(dkorolev): Resolve geolocation from IP?
};

class EventCollectorHTTPServer {
 public:
  EventCollectorHTTPServer(int http_port,
                           std::ostream& ostream,
                           std::chrono::microseconds tick_interval_us,
                           const std::string& route = "/log",
                           const std::string& response_text = "OK\n",
                           std::function<void(const LogEntryWithHeaders&)> callback = {})
      : http_port_(http_port),
        ostream_(ostream),
        route_(route),
        response_text_(response_text),
        callback_(callback),
        tick_interval_us_(tick_interval_us),
        send_ticks_(tick_interval_us_.count() > 0),
        last_event_t_(0u),
        events_pushed_(0u),
        timer_thread_(&EventCollectorHTTPServer::TimerThreadFunction, this),
        http_route_scope_(HTTP(http_port_)
                              .Register(route_,
                                        [this](Request r) {
                                          LogEntryWithHeaders entry;
                                          {
                                            const auto now = current::time::Now();
                                            std::lock_guard<std::mutex> lock(mutex_);
                                            entry.t = now.count();
                                            entry.m = r.method;
                                            entry.u = r.url.ComposeURLWithoutParameters();
                                            entry.q = r.url.AllQueryParameters();
                                            entry.h = r.headers.AsMap();
                                            entry.c = r.headers.CookiesAsString();
                                            entry.b = r.body;
                                            entry.f = r.url.fragment;
                                            ostream_ << JSON(entry) << std::endl;
                                            ++events_pushed_;
                                            last_event_t_ = now;
                                            if (callback_) {
                                              callback_(entry);
                                            }
                                          }
                                          r(response_text_);
                                        })) {}

  EventCollectorHTTPServer(const EventCollectorHTTPServer&) = delete;
  EventCollectorHTTPServer(EventCollectorHTTPServer&&) = delete;
  void operator=(const EventCollectorHTTPServer&) = delete;
  void operator=(EventCollectorHTTPServer&&) = delete;

  ~EventCollectorHTTPServer() {
    send_ticks_ = false;
    timer_thread_.join();
  }

  void Join() { HTTP(http_port_).Join(); }

  void TimerThreadFunction() {
    { std::lock_guard<std::mutex> lock(mutex_); }
    while (send_ticks_) {
      const std::chrono::microseconds sleep_us = [&]() {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::chrono::microseconds now = current::time::Now();
        const std::chrono::microseconds dt = now - last_event_t_;
        if (dt >= tick_interval_us_) {
          LogEntryWithHeaders entry;
          entry.t = now.count();
          entry.m = "TICK";
          ostream_ << JSON(entry) << std::endl;
          ++events_pushed_;
          last_event_t_ = now;
          if (callback_) {
            callback_(entry);
          }
          return std::chrono::microseconds(0);
        } else {
          return tick_interval_us_ - dt + std::chrono::microseconds(1);
        }
      }();
      if (sleep_us.count()) {
        std::this_thread::sleep_for(sleep_us);
      }
    }
  }

  size_t EventsPushed() const { return events_pushed_; }

 private:
  std::mutex mutex_;
  const int http_port_;
  std::ostream& ostream_;
  const std::string route_;
  const std::string response_text_;
  std::function<void(const LogEntryWithHeaders&)> callback_;

  const std::chrono::microseconds tick_interval_us_;
  std::atomic_bool send_ticks_;
  std::chrono::microseconds last_event_t_;
  std::atomic_size_t events_pushed_;
  std::thread timer_thread_;
  HTTPRoutesScope http_route_scope_;
};

#endif  // EVENT_COLLECTOR_H
