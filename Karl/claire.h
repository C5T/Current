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

// NOTE: The user must `#include` their local `current_build.h` header prior to Karl/Claire headers.

#ifndef KARL_CLAIRE_H
#define KARL_CLAIRE_H

#include "../port.h"

// The `current_build.h` file from this local `Current/Karl` dir makes no sense for external users of Karl.
// Nonetheless, top-level `make test` and `make check` should pass out of the box.
#ifdef CURRENT_MAKE_CHECK_MODE
#include "current_build.h.mock"
#endif

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "schema_claire.h"
#include "locator.h"
#include "exceptions.h"
#include "respond_with_schema.h"

#include "../Blocks/HTTP/api.h"

#include "../Bricks/time/chrono.h"
#include "../Bricks/util/random.h"

namespace current {
namespace karl {

// No need for `CURRENT_FIELD_DESCRIPTION`-s in this structure. -- D.K.
CURRENT_STRUCT(InternalKeepaliveAttemptResult) {
  CURRENT_FIELD(timestamp, std::chrono::microseconds);
  CURRENT_FIELD(status, KeepaliveAttemptStatus, KeepaliveAttemptStatus::Unknown);
  CURRENT_FIELD(http_code, uint16_t, static_cast<uint16_t>(net::HTTPResponseCodeValue::InvalidCode));
};

inline std::string KeepaliveAttemptResultAsString(const InternalKeepaliveAttemptResult& result) {
  if (result.status == KeepaliveAttemptStatus::Unknown) {
    return "Unknown";
  } else if (result.status == KeepaliveAttemptStatus::Success) {
    return "Success";
  } else if (result.status == KeepaliveAttemptStatus::CouldNotConnect) {
    return "HTTP connection attempt failed";
  } else if (result.status == KeepaliveAttemptStatus::ErrorCodeReturned) {
    return "HTTP response code " + current::ToString(result.http_code);
  } else {
    return "Error: " + JSON(result);
  }
}

// Interface to implement for receiving callbacks/notifications from Claire.
class IClaireNotifiable {
 public:
  virtual ~IClaireNotifiable() = default;
  // Claire has been requested to change its Karl locator.
  virtual void OnKarlLocatorChanged(const Locator& locator) = 0;
};

// Dummy class with no-op functions. Used by Claire if no custom notifiable class has been provided.
class DummyClaireNotifiable : public IClaireNotifiable {
 public:
  void OnKarlLocatorChanged(const Locator&) override {}
};

template <class T>
class GenericClaire final : private DummyClaireNotifiable {
 public:
  using specific_status_t = ClaireServiceStatus<T>;
  using status_generator_t = std::function<T()>;

  GenericClaire(Locator karl,
                const std::string& service,
                uint16_t port,
                IClaireNotifiable& notifiable,
                std::vector<std::string> dependencies = std::vector<std::string>())
      : in_beacon_mode_(false),
        karl_(karl),
        service_(service),
        codename_(GenerateRandomCodename()),
        port_(port),
        dependencies_(ParseDependencies(dependencies)),
        karl_keepalive_route_(KarlKeepaliveRoute(karl_, codename_, port_)),
        notifiable_ref_(notifiable),
        us_start_(current::time::Now()),
        http_scope_(HTTP(port).Register("/.current", [this](Request r) { ServeCurrent(std::move(r)); })),
        keepalive_thread_terminating_(false) {}

  GenericClaire(Locator karl,
                const std::string& service,
                uint16_t port,
                std::vector<std::string> dependencies = std::vector<std::string>())
      : GenericClaire(karl, service, port, *this, dependencies) {}

  GenericClaire() = delete;

  virtual ~GenericClaire() {
    if (keepalive_thread_.joinable()) {
      keepalive_thread_terminating_ = true;
      keepalive_condition_variable_.notify_one();
      keepalive_thread_.join();
      // Deregister self from Karl.
      try {
        HTTP(DELETE(karl_keepalive_route_));
      } catch (net::NetworkException&) {
      }
    }
  }

  void AddDependency(const std::string& service) { dependencies_.insert(service); }

  void AddDependency(const ClaireServiceKey& service) { dependencies_.insert(service); }

  void RemoveDependency(const std::string& service) { dependencies_.erase(service); }

  void RemoveDependency(const ClaireServiceKey& service) { dependencies_.erase(service); }

  void Register(status_generator_t status_filler = nullptr, bool require_karls_confirmation = false) {
    // Register this Claire with Karl and spawn the thread to send regular keepalives.
    // If `require_karls_confirmation` is true, throw if Karl can not be reached.
    // If `require_karls_confirmation` is false, just start the keepalives thread.
    std::unique_lock<std::mutex> lock(keepalive_mutex_);
    if (!in_beacon_mode_) {
      {
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_generator_ = status_filler;
      }

      if (require_karls_confirmation) {
        const std::string route = karl_keepalive_route_ + "&confirm";
        // The call to `SendKeepaliveToKarl` is blocking.
        // With "&confirm" at the end, the call to Karl would require Karl calling Claire back.
        // Can throw, the exception should propagate up.
        SendKeepaliveToKarl(lock, karl_keepalive_route_ + "&confirm");
      }

      StartKeepaliveThread();
      in_beacon_mode_ = true;
    }
  }

  const std::string& Codename() const { return codename_; }

  Locator GetKarlLocator() const {
    std::lock_guard<std::mutex> lock(keepalive_mutex_);
    return karl_;
  }

  void SetKarlLocator(const Locator& new_karl_locator) {
    {
      std::lock_guard<std::mutex> lock(keepalive_mutex_);
      karl_ = new_karl_locator;
      karl_keepalive_route_ = KarlKeepaliveRoute(karl_, codename_, port_);
      notifiable_ref_.OnKarlLocatorChanged(new_karl_locator);
    }
    keepalive_condition_variable_.notify_one();
  }

  ClaireStatus& BoilerplateStatus() { return boilerplate_status_; }

 private:
  static std::string GenerateRandomCodename() {
    std::string codename;
    for (int i = 0; i < 6; ++i) {
      codename += static_cast<char>(current::random::CSRandomInt('A', 'Z'));
    }
    return codename;
  }

  static std::string KarlKeepaliveRoute(const Locator& karl_locator,
                                        const std::string& codename,
                                        uint16_t port) {
    return karl_locator.address_port_route + "?codename=" + codename + "&port=" + current::ToString(port);
  }

  static std::set<ClaireServiceKey> ParseDependencies(const std::vector<std::string>& dependencies) {
    std::vector<ClaireServiceKey> result;
    for (const auto& dependency : dependencies) {
      result.push_back(ClaireServiceKey(dependency));
    }
    return std::set<ClaireServiceKey>(result.begin(), result.end());
  }

  void FillBaseKeepaliveStatus(ClaireStatus& status, bool fill_current_build = true) const {
    const auto now = current::time::Now();

#ifndef CURRENT_MOCK_TIME
    // With mock time, can result in trouble. -- D.K.
    assert(now >= us_start_);
#endif

    status.service = service_;
    status.codename = codename_;
    status.start_time_epoch_microseconds = us_start_;

    status.local_port = port_;
    status.dependencies.assign(dependencies_.begin(), dependencies_.end());

    status.now = now;

#ifndef CURRENT_MOCK_TIME
    // With mock time, can result in negatives in `status.uptime`, which would kill `ParseJSON`. -- D.K.
    status.uptime = current::strings::TimeIntervalAsHumanReadableString(now - us_start_);

    {
      std::lock_guard<std::mutex> lock(status_mutex_);
      if (last_keepalive_attempt_result_.timestamp.count()) {
        status.last_keepalive_sent = current::strings::TimeIntervalAsHumanReadableString(
                                         now - last_keepalive_attempt_result_.timestamp) +
                                     " ago";
        status.last_keepalive_status = KeepaliveAttemptResultAsString(last_keepalive_attempt_result_);
      }
      if (last_successful_keepalive_timestamp_.count()) {
        status.last_successful_keepalive =
            current::strings::TimeIntervalAsHumanReadableString(now - last_successful_keepalive_timestamp_) +
            " ago";
      }
      if (last_successful_keepalive_ping_.count()) {
        status.last_successful_keepalive_ping_us = last_successful_keepalive_ping_;
        status.last_successful_keepalive_ping =
            current::strings::Printf("%.2lfms", 1e-3 * last_successful_keepalive_ping_.count());
      }
    }
#endif

    if (fill_current_build) {
      status.build = build::BuildInfo();
    }
  }

  void FillKeepaliveStatus(specific_status_t& status, bool fill_current_build = true) const {
    FillBaseKeepaliveStatus(status, fill_current_build);
    {
      std::lock_guard<std::mutex> lock(status_mutex_);
      if (status_generator_) {
        status.runtime = status_generator_();
      }
    }
  }

  specific_status_t GenerateKeepaliveStatus(bool fill_current_build = true) const {
    specific_status_t status(boilerplate_status_);
    FillKeepaliveStatus(status, fill_current_build);
    return status;
  }

  void StartKeepaliveThread() {
    keepalive_thread_ = std::thread([this]() { KeepaliveThread(); });
  }

  // Sends a keepalive message to Karl.
  // Blocking, and can throw.
  // Possibly via a custom `route`: adding "&confirm", for example, would require Karl to crawl Claire back.
  void SendKeepaliveToKarl(std::unique_lock<std::mutex>&, const std::string& route) {
    // Basically, throw in case of any error, and throw only one type: `ClaireRegistrationException`.
    const auto keepalive_body = GenerateKeepaliveStatus();

    try {
      {
        std::lock_guard<std::mutex> lock(status_mutex_);
        last_keepalive_attempt_result_.timestamp = current::time::Now();
        last_keepalive_attempt_result_.status = KeepaliveAttemptStatus::Unknown;
        last_keepalive_attempt_result_.http_code =
            static_cast<uint16_t>(net::HTTPResponseCodeValue::InvalidCode);
      }

      const auto code = HTTP(POST(route, keepalive_body)).code;

      {
        std::lock_guard<std::mutex> lock(status_mutex_);
        if (static_cast<int>(code) >= 200 && static_cast<int>(code) <= 299) {
          // Success. And anything else is failure.
          last_keepalive_attempt_result_.status = KeepaliveAttemptStatus::Success;
          last_successful_keepalive_timestamp_ = last_keepalive_attempt_result_.timestamp;
          last_successful_keepalive_ping_ = current::time::Now() - last_keepalive_attempt_result_.timestamp;
          return;
        } else {
          last_keepalive_attempt_result_.status = KeepaliveAttemptStatus::ErrorCodeReturned;
          last_keepalive_attempt_result_.http_code = static_cast<uint16_t>(code);
        }
      }
    } catch (const current::Exception&) {
      last_keepalive_attempt_result_.status = KeepaliveAttemptStatus::CouldNotConnect;
    }
    // OK to throw here. In the thread that sends repeated keepalives, this exception would be caught,
    // and if an exception is thrown during the initial registration, it is an emergency unless the user
    // decides otherwise.
    CURRENT_THROW(ClaireRegistrationException(service_, route));
  }

  // The semantic to ensure keepalives only happen from a locked section.
  void SendKeepaliveToKarl(const std::string& route) {
    std::unique_lock<std::mutex> lock(keepalive_mutex_);
    SendKeepaliveToKarl(lock, route);
  }

  // The thread sends periodic keepalive messages.
  void KeepaliveThread() {
    while (!keepalive_thread_terminating_) {
      std::unique_lock<std::mutex> lock(keepalive_mutex_);

      // Have the interval normalized a bit.
      // TODO(dkorolev): Parameter or named constant for keepalive frequency?
      const std::chrono::microseconds projected_next_keepalive =
          last_keepalive_attempt_result_.timestamp +
          std::chrono::microseconds(current::random::CSRandomUInt64(20e6 * 0.9, 20e6 * 1.1));

      const std::chrono::microseconds now = current::time::Now();

      if (projected_next_keepalive > now) {
        keepalive_condition_variable_.wait_for(lock, projected_next_keepalive - now);
      }

      if (keepalive_thread_terminating_) {
        return;
      }

      try {
        SendKeepaliveToKarl(lock, karl_keepalive_route_);
      } catch (const ClaireRegistrationException&) {
        // Ignore exceptions if there's a problem talking to Karl. He'll come back. He's Karl.
      }
    }
  }

  void ServeCurrent(Request r) {
    if (r.method == "GET") {
      const auto& qs = r.url.query;
      if (qs.has("schema")) {
        const auto& schema = qs["schema"];
        using L = current::reflection::Language;
        if (schema == "md") {
          RespondWithSchema<specific_status_t, L::Markdown>(std::move(r));
        } else if (schema == "fs") {
          RespondWithSchema<specific_status_t, L::FSharp>(std::move(r));
        } else {
          RespondWithSchema<specific_status_t, L::JSON>(std::move(r));
        }
      } else {
        const bool all = qs.has("all") || qs.has("a");
        const bool build = qs.has("build") || qs.has("b");
        const bool runtime = qs.has("runtime") || qs.has("r");
        if (!all && build) {
          r(build::BuildInfo());
        } else if (!all && runtime) {
          r(([this]() -> Response {
            std::lock_guard<std::mutex> lock(status_mutex_);
            if (status_generator_) {
              return Response(status_generator_());
            } else {
              return Response("Not ready.\n", HTTPResponseCode.ServiceUnavailable);
            }
          })());
        } else {
          // Don't use `JSONFormat::Minimalistic` to support type evolution
          // of how to report/aggregate/render statuses on the Karl side.
          r(GenerateKeepaliveStatus(all));
        }
      }
    } else if (r.method == "POST") {
      if (r.url.query.has("report_to") && !r.url.query["report_to"].empty()) {
        URL decomposed_karl_url(r.url.query["report_to"]);
        if (!decomposed_karl_url.host.empty()) {
          const std::string karl_url = decomposed_karl_url.ComposeURL();
          SetKarlLocator(Locator(karl_url));
          r("Now reporting to '" + karl_url + "'.\n");
        } else {
          r("Valid URL parameter `report_to` required.\n", HTTPResponseCode.BadRequest);
        }
      } else {
        r("Valid URL parameter `report_to` required.\n", HTTPResponseCode.BadRequest);
      }
    } else {
      r(current::net::DefaultMethodNotAllowedMessage(),
        HTTPResponseCode.MethodNotAllowed,
        net::constants::kDefaultHTMLContentType);
    }
  }

 private:
  std::atomic_bool in_beacon_mode_;

  Locator karl_;
  const std::string service_;
  const std::string codename_;
  const int port_;
  std::set<ClaireServiceKey> dependencies_;
  std::string karl_keepalive_route_;
  IClaireNotifiable& notifiable_ref_;

  const std::chrono::microseconds us_start_;

  mutable std::mutex status_mutex_;
  status_generator_t status_generator_;
  ClaireStatus boilerplate_status_;
  InternalKeepaliveAttemptResult last_keepalive_attempt_result_;
  std::chrono::microseconds last_successful_keepalive_timestamp_ = std::chrono::microseconds(0);
  std::chrono::microseconds last_successful_keepalive_ping_ = std::chrono::microseconds(0);

  const HTTPRoutesScope http_scope_;

  std::atomic_bool keepalive_thread_terminating_;
  mutable std::mutex keepalive_mutex_;
  std::condition_variable keepalive_condition_variable_;
  std::thread keepalive_thread_;
};

using Claire = GenericClaire<Variant<default_user_status::status>>;

}  // namespace current::karl
}  // namespace current

#endif  // KARL_CLAIRE_H
