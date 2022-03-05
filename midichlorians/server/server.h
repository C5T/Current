/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef CURRENT_MIDICHLORIANS_SERVER_H
#define CURRENT_MIDICHLORIANS_SERVER_H

#include "schema.h"

#include "../../typesystem/serialization/json.h"

#include "../../blocks/http/api.h"
#include "../../bricks/strings/strings.h"
#include "../../bricks/time/chrono.h"

namespace current {
namespace midichlorians {
namespace server {

template <class LOG_ENTRY_CONSUMER>
class midichloriansHTTPServer {
 public:
  midichloriansHTTPServer(int http_port,
                          LOG_ENTRY_CONSUMER& log_entry_consumer,
                          std::chrono::microseconds tick_interval_us,
                          const std::string& route = "/log",
                          const std::string& response_text = "OK\n")
      : http_port_(http_port),
        log_entry_consumer_(log_entry_consumer),
        route_(route),
        response_text_(response_text),
        tick_interval_us_(tick_interval_us),
        send_ticks_(tick_interval_us_.count() > 0),
        last_event_t_(0u),
        events_pushed_(0u),
        timer_thread_(&midichloriansHTTPServer::TimerThreadFunction, this),
        routes_(HTTP(http_port_).Register(route_, [this](Request r) { ProcessRequest(std::move(r)); })) {}

  ~midichloriansHTTPServer() {
    send_ticks_ = false;
    timer_thread_.join();
  }

  void Join() { HTTP(http_port_).Join(); }

  size_t EventsPushed() const { return events_pushed_; }

 private:
  void ProcessRequest(Request r) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (r.method == "POST" && !r.body.empty()) {
        // POST request could be either iOS or web event.
        // In case of iOS there could be a bunch of events in the body.
        std::istringstream iss(r.body);
        std::string line;
        while (std::getline(iss, line)) {
          if (!ParseBodyLineAsiOSEvent(line)) {
            if (!ParseRequestAsWebEvent(r)) {
              LogUnparsableRequest(r);
            }
          }
        }
      } else if (r.method == "GET" || r.method == "HEAD") {
        // GET and HEAD requests could be only web events.
        if (!ParseRequestAsWebEvent(r)) {
          LogUnparsableRequest(r);  // LCOV_EXCL_LINE
        }
      } else {
        // Wrong HTTP method or empty body in POST request.
        LogUnparsableRequest(r);  // LCOV_EXCL_LINE
      }
    }
    r(response_text_);
  }

  bool ParseBodyLineAsiOSEvent(const std::string& line) {
    using namespace current::midichlorians::ios;

    Variant<ios_events_t> ios_event;
    try {
      ios_event = ParseJSON<Variant<ios_events_t>>(line);
    } catch (const InvalidJSONException&) {
      return false;
    }

    PassLogEntryToConsumer(EventLogEntry(current::time::Now(), std::move(ios_event)));
    return true;
  }

  bool ParseRequestAsWebEvent(const Request& r) {
    using namespace current::midichlorians::web;

    std::map<std::string, std::string> extracted_q;  // Manually extracted query parameters.
    const std::map<std::string, std::string>& h = r.headers.AsMap();
    const std::map<std::string, current::net::http::Cookie>& c = r.headers.cookies;

    bool is_allowed_method = false;
    const std::map<std::string, std::string>& q =
        [&r, &extracted_q, &is_allowed_method]() -> const std::map<std::string, std::string>& {
      if (r.method == "GET" || r.method == "HEAD") {
        is_allowed_method = true;
        return r.url.AllQueryParameters();
      } else if (r.method == "POST") {
        is_allowed_method = true;
        extracted_q = current::url::URL::ParseQueryString(r.body);
        for (const auto& cit : r.url.AllQueryParameters()) {
          extracted_q.insert(cit);
        }
      }
      return extracted_q;
    }();

    if (!is_allowed_method) {
      return false;  // LCOV_EXCL_LINE
    }

    Variant<web_events_t> web_event;
    try {
      if (q.at("ea") == "En") {
        web_event = WebEnterEvent();
      } else if (q.at("ea") == "Ex") {
        web_event = WebExitEvent();
      } else if (q.at("ea") == "Fg") {
        web_event = WebForegroundEvent();
      } else if (q.at("ea") == "Bg") {
        web_event = WebBackgroundEvent();
      } else {
        web_event = WebGenericEvent(q.at("ec"), q.at("ea"));
      }
    } catch (const std::out_of_range&) {
      return false;
    }

    if (!ExtractWebBaseEventFields(q, h, c, web_event)) {
      return false;  // LCOV_EXCL_LINE
    }

    PassLogEntryToConsumer(EventLogEntry(current::time::Now(), std::move(web_event)));
    return true;
  }

  bool ExtractWebBaseEventFields(const std::map<std::string, std::string>& q,
                                 const std::map<std::string, std::string>& h,
                                 const std::map<std::string, current::net::http::Cookie>& c,
                                 Variant<web_events_t>& web_event) {
    using namespace current::midichlorians::web;

    static_cast<void>(c);  // Ignore cookies for now. -- D.K.

    WebBaseEvent& dest_event = Value<WebBaseEvent>(web_event);
    try {
      dest_event.user_ms = std::chrono::milliseconds(current::FromString<uint64_t>(q.at("_t")));
      dest_event.customer_id = q.at("CUSTOMER_ACCOUNT");
      dest_event.client_id = q.at("cid");
      dest_event.ip = h.at("X-Forwarded-For");
      dest_event.user_agent = h.at("User-Agent");
      const auto url = URL(h.at("Referer"));
      dest_event.referer_host = url.host;
      dest_event.referer_path = url.path;
      dest_event.referer_querystring = url.query.AsImmutableMap();
    } catch (const std::out_of_range&) {  // LCOV_EXCL_LINE
      // TODO(mz+dk): discuss the validity of partially filled event fields.
      return false;                            // LCOV_EXCL_LINE
    } catch (const url::EmptyURLException&) {  // LCOV_EXCL_LINE
      return false;                            // LCOV_EXCL_LINE
    }
    return true;
  }

  template <typename LOG_ENTRY>
  void PassLogEntryToConsumer(LOG_ENTRY&& entry, bool is_valid_entry = true) {
    log_entry_variant_t entry_variant(std::move(entry));
    entry_variant.Call(log_entry_consumer_);
    if (is_valid_entry) {
      ++events_pushed_;
      last_event_t_ = Value<LogEntryBase>(entry_variant).server_us;
    }
  }

  void LogUnparsableRequest(const Request& r) {
    PassLogEntryToConsumer(UnparsableLogEntry(current::time::Now(), r), false);
  }

  void TimerThreadFunction() {
    // TODO(dkorolev): Use "cron" here.
    while (send_ticks_) {
      std::unique_lock<std::mutex> lock(mutex_);
      const std::chrono::microseconds now = current::time::Now();
      const std::chrono::microseconds dt = now - last_event_t_;
      if (dt >= tick_interval_us_) {
        PassLogEntryToConsumer(TickLogEntry(now));
      } else {
        lock.unlock();
        std::this_thread::sleep_for(tick_interval_us_ - dt + std::chrono::microseconds(1));
      }
    }
  }

 private:
  std::mutex mutex_;
  const int http_port_;
  LOG_ENTRY_CONSUMER& log_entry_consumer_;
  const std::string route_;
  const std::string response_text_;

  const std::chrono::microseconds tick_interval_us_;
  std::atomic_bool send_ticks_;
  std::chrono::microseconds last_event_t_;
  std::atomic_size_t events_pushed_;
  std::thread timer_thread_;

  HTTPRoutesScope routes_;
};

}  // namespace server
}  // namespace midichlorians
}  // namespace current

#endif  // CURRENT_MIDICHLORIANS_SERVER_H
