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

#include "../../TypeSystem/Serialization/json.h"

#include "../../Blocks/HTTP/api.h"
#include "../../Bricks/strings/strings.h"
#include "../../Bricks/time/chrono.h"

namespace current {
namespace midichlorians {
namespace server {

template <class LOG_ENTRY_CONSUMER>
class MidichloriansHTTPServer {
 public:
  MidichloriansHTTPServer(int http_port,
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
        timer_thread_(&MidichloriansHTTPServer::TimerThreadFunction, this),
        routes_(HTTP(http_port_).Register(route_, [this](Request r) { ProcessRequest(std::move(r)); })) {}

  ~MidichloriansHTTPServer() {
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
          if (!BodyLineAsiOSEvent(line)) {
            if (!RequestAsWebEvent(r)) {
              LogInvalidRequest(r);
            }
          }
        }
      } else if (r.method == "GET" || r.method == "HEAD") {
        // GET and HEAD requests could be only web events.
        if (!RequestAsWebEvent(r)) {
          LogInvalidRequest(r);
        }
      } else {
        LogInvalidRequest(r);
      }
    }
    r(response_text_);
  }

  bool BodyLineAsiOSEvent(const std::string& line) {
    using namespace current::midichlorians::ios;

    Variant<T_IOS_EVENTS> ios_event;
    try {
      ios_event = ParseJSON<Variant<T_IOS_EVENTS>>(line);
    } catch (const InvalidJSONException&) {
      return false;
    }

    PassLogEntryToConsumer(EventLogEntry(current::time::Now(), std::move(ios_event)));
    return true;
  }

  bool RequestAsWebEvent(const Request& r) {
    using namespace current::midichlorians::web;

    std::map<std::string, std::string> post_q;  // Query parameters extracted from POST body.
    const std::map<std::string, std::string>* q;
    const std::map<std::string, std::string>* h = &r.headers;

    if (r.method == "GET" || r.method == "HEAD") {
      q = &r.url.AllQueryParameters();
    } else if (r.method == "POST") {
      post_q = blocks::impl::URLParametersExtractor("?" + r.body).AllQueryParameters();
      for (const auto& cit : r.url.AllQueryParameters()) {
        post_q.insert(cit);
      }
      q = &post_q;
    } else {
      return false;
    }

    Variant<T_WEB_EVENTS> web_event;
    try {
      if (q->at("ea") == "En") {
        web_event = WebEnterEvent();
      } else if (q->at("ea") == "Ex") {
        web_event = WebExitEvent();
      } else if (q->at("ea") == "Fg") {
        web_event = WebForegroundEvent();
      } else if (q->at("ea") == "Bg") {
        web_event = WebBackgroundEvent();
      } else {
        web_event = WebGenericEvent(q->at("ec"), q->at("ea"));
      }
    } catch (const std::out_of_range&) {
      return false;
    }

    if (!ExtractWebBaseEventFields(q, h, web_event)) {
      return false;
    }

    PassLogEntryToConsumer(EventLogEntry(current::time::Now(), std::move(web_event)));
    return true;
  }

  template <typename T_LOG_ENTRY>
  void PassLogEntryToConsumer(T_LOG_ENTRY&& entry, bool is_valid_entry = true) {
    T_LOG_ENTRY_VARIANT entry_variant(std::move(entry));
    entry_variant.Call(log_entry_consumer_);
    if (is_valid_entry) {
      ++events_pushed_;
      last_event_t_ = Value<LogEntryBase>(entry_variant).server_us;
    }
  }

  bool ExtractWebBaseEventFields(const std::map<std::string, std::string>* q,
                                 const std::map<std::string, std::string>* h,
                                 Variant<T_WEB_EVENTS>& web_event) {
    using namespace current::midichlorians::web;

    assert(q);
    assert(h);

    WebBaseEvent& dest_event = Value<WebBaseEvent>(web_event);
    try {
      dest_event.user_ms = std::chrono::milliseconds(strings::FromString<uint64_t>(q->at("_t")));
      dest_event.customer_id = q->at("CUSTOMER_ACCOUNT");
      dest_event.client_id = q->at("cid");
      dest_event.ip = h->at("X-Forwarded-For");
      dest_event.user_agent = h->at("User-Agent");
      blocks::URL url = blocks::URL(h->at("Referer"));
      dest_event.referer_host = url.host;
      dest_event.referer_path = url.path;
      dest_event.referer_querystring = url.query.AsImmutableMap();
    } catch (const std::out_of_range&) {
      return false;
    } catch (const blocks::EmptyURLException&) {
      return false;
    }
    return true;
  }

  void LogInvalidRequest(const Request& r) {
    PassLogEntryToConsumer(InvalidLogEntry(current::time::Now(), r), false);
  }

  void TimerThreadFunction() {
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
