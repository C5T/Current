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

#include "../blocks/http/api.h"

#include "../bricks/time/chrono.h"
#include "../bricks/sync/locks.h"
#include "../bricks/util/random.h"

namespace current {
namespace karl {

enum class ForceSendKeepaliveWaitRequest : bool { DoNotWait = false, Wait = true };

// No need for `CURRENT_FIELD_DESCRIPTION`-s in this structure. -- D.K.
CURRENT_STRUCT(InternalKeepaliveAttemptResult) {
  CURRENT_FIELD(timestamp, std::chrono::microseconds, std::chrono::microseconds(0));
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
        destructing_(false),
        keepalive_thread_force_wakeup_(false),
        keepalive_thread_running_(false) {}

  GenericClaire(Locator karl,
                const std::string& service,
                uint16_t port,
                std::vector<std::string> dependencies = std::vector<std::string>())
      : GenericClaire(karl, service, port, *this, dependencies) {}

  GenericClaire() = delete;

  virtual ~GenericClaire() {
    std::unique_lock<std::mutex> lock(keepalive_thread_running_status_mutex_);

    // Signal the keepalive thread to shut down if running, and prevent the one one from starting.
    destructing_ = true;

    if (keepalive_thread_running_) {
      {
        std::lock_guard<std::mutex> keepalive_mutex_lock(keepalive_mutex_);
        keepalive_condition_variable_.notify_all();
      }
      lock.unlock();
      keepalive_thread_.join();
      CURRENT_ASSERT(!keepalive_thread_running_);
      try {
        // Deregister self from Karl.
        HTTP(DELETE(karl_keepalive_route_));
      } catch (net::NetworkException&) {  // LCOV_EXCL_LINE
      }
    }
  }

  void AddDependency(const std::string& service) {
    std::unique_lock<std::mutex> lock(keepalive_mutex_);
    dependencies_.insert(service);
  }

  void AddDependency(const ClaireServiceKey& service) {
    std::unique_lock<std::mutex> lock(keepalive_mutex_);
    dependencies_.insert(service);
  }

  void SetDependencies(const std::vector<std::string>& services) {
    std::unique_lock<std::mutex> lock(keepalive_mutex_);
    dependencies_ = ParseDependencies(services);
  }

  void RemoveDependency(const std::string& service) {
    std::unique_lock<std::mutex> lock(keepalive_mutex_);
    dependencies_.erase(service);
  }

  void RemoveDependency(const ClaireServiceKey& service) {
    std::unique_lock<std::mutex> lock(keepalive_mutex_);
    dependencies_.erase(service);
  }

  void Register(status_generator_t status_filler = nullptr, bool require_karls_confirmation = false) {
    // Register this Claire with Karl and spawn the thread to send regular keepalives.
    // If `require_karls_confirmation` is true, throw if Karl can not be reached.
    // If `require_karls_confirmation` is false, just start the keepalives thread.
    std::unique_lock<std::mutex> lock(keepalive_mutex_);
    if (!in_beacon_mode_) {
      {
        std::lock_guard<std::mutex> inner_lock(status_mutex_);
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

  void ForceSendKeepalive(ForceSendKeepaliveWaitRequest wait_for_keepalive = ForceSendKeepaliveWaitRequest::DoNotWait) {
    {
      std::lock_guard<std::mutex> lock(keepalive_thread_running_status_mutex_);
      if (!keepalive_thread_running_) {
        StartKeepaliveThread<current::locks::MutexLockStatus::AlreadyLocked>();
      }
    }

    // Generally, unnecessary. For debugging / troubleshooting purposes. -- D.K.
    if (wait_for_keepalive == ForceSendKeepaliveWaitRequest::Wait) {
      std::lock_guard<std::mutex> lock(keepalive_mutex_);
      keepalive_sent_ = false;
    }

    keepalive_thread_force_wakeup_ = true;
    {
      std::lock_guard<std::mutex> lock(keepalive_mutex_);
      keepalive_condition_variable_.notify_all();
    }

    // Generally, unnecessary. For debugging / troubleshooting purposes. -- D.K.
    if (wait_for_keepalive == ForceSendKeepaliveWaitRequest::Wait) {
      std::unique_lock<std::mutex> lock(keepalive_mutex_);
      while (!keepalive_sent_) {
        keepalive_condition_variable_.wait_for(
            lock, std::chrono::milliseconds(1), [this]() { return keepalive_sent_; });
      }
    }
  }

  Locator GetKarlLocator() const {
    std::lock_guard<std::mutex> lock(keepalive_mutex_);
    return karl_;
  }

  void SetKarlLocator(const Locator& new_karl_locator,
                      ForceSendKeepaliveWaitRequest wait_for_keepalive = ForceSendKeepaliveWaitRequest::DoNotWait) {
    {
      std::lock_guard<std::mutex> lock(keepalive_mutex_);
      karl_ = new_karl_locator;
      karl_keepalive_route_ = KarlKeepaliveRoute(karl_, codename_, port_);
      notifiable_ref_.OnKarlLocatorChanged(new_karl_locator);
    }
    ForceSendKeepalive(wait_for_keepalive);
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

  static std::string KarlKeepaliveRoute(const Locator& karl_locator, const std::string& codename, uint16_t port) {
    return karl_locator.address_port_route + "?codename=" + codename + "&port=" + current::ToString(port);
  }

  static std::set<ClaireServiceKey> ParseDependencies(const std::vector<std::string>& dependencies) {
    std::set<ClaireServiceKey> result;
    for (const auto& dependency : dependencies) {
      result.emplace(dependency);
    }
    return result;
  }

  void FillBaseKeepaliveStatus(ClaireStatus& status, bool fill_current_build = true) const {
    const auto now = current::time::Now();

#ifndef CURRENT_MOCK_TIME
    // With mock time, can result in trouble. -- D.K.
    CURRENT_ASSERT(now >= us_start_);
#endif

    status.service = service_;
    status.codename = codename_;
    status.start_time_epoch_microseconds = us_start_;

    status.local_port = port_;
    status.dependencies.assign(dependencies_.begin(), dependencies_.end());
    status.reporting_to = karl_.address_port_route;

    status.now = now;

#ifndef CURRENT_MOCK_TIME
    // With mock time, can result in negatives in `status.uptime`, which would kill `ParseJSON`. -- D.K.
    status.uptime = current::strings::TimeIntervalAsHumanReadableString(now - us_start_);

    {
      std::lock_guard<std::mutex> lock(status_mutex_);
      if (last_keepalive_attempt_result_.timestamp.count()) {
        status.last_keepalive_sent =
            current::strings::TimeIntervalAsHumanReadableString(now - last_keepalive_attempt_result_.timestamp) +
            " ago";
        status.last_keepalive_status = KeepaliveAttemptResultAsString(last_keepalive_attempt_result_);
      }
      if (last_successful_keepalive_timestamp_.count()) {
        status.last_successful_keepalive =
            current::strings::TimeIntervalAsHumanReadableString(now - last_successful_keepalive_timestamp_) + " ago";
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

  template <typename current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock>
  void StartKeepaliveThread() {
    current::locks::SmartMutexLockGuard<MLS> lock(keepalive_thread_running_status_mutex_);
    if (destructing_) {
      return;  // LCOV_EXCL_LINE
    }
    if (!keepalive_thread_running_) {
      keepalive_thread_ = std::thread([this]() {
        keepalive_thread_running_ = true;
        KeepaliveThread();
      });
      while (!keepalive_thread_running_) {
        // `StartKeepaliveThread()` is a rare operation, and it may take a while on an overloaded CPU.
        // Thus, `std::this_thread::yield()` would be an overkill, we want this code to behave on a busy system.
        // A condition variable or a `WaitableAtomic` is a cleaner solution here; for now, just sleep.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    } else {
      // TODO(dkorolev), #FIXME_DIMA: This should not happen.
      // LCOV_EXCL_START
      std::cerr << "Already have `keepalive_thread_running_` in Claire::StartKeepaliveThread().\n";
      std::exit(-1);
      // LCOV_EXCL_STOP
    }
  }

  // Sends a keepalive message to Karl.
  // Blocking, and can throw.
  // Possibly via a custom `route`: adding "&confirm", for example, would require Karl to crawl Claire back.
  void SendKeepaliveToKarl(std::unique_lock<std::mutex>&, const std::string& route) {
    // Basically, throw in case of any error, and throw only one type: `ClaireRegistrationException`.
    const auto keepalive_body = GenerateKeepaliveStatus();

    std::string error_message = "";

    try {
      {
        std::lock_guard<std::mutex> lock(status_mutex_);
        last_keepalive_attempt_result_.timestamp = current::time::Now();
        last_keepalive_attempt_result_.status = KeepaliveAttemptStatus::Unknown;
        last_keepalive_attempt_result_.http_code = static_cast<uint16_t>(net::HTTPResponseCodeValue::InvalidCode);
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
    } catch (const current::Exception& e) {
      last_keepalive_attempt_result_.status = KeepaliveAttemptStatus::CouldNotConnect;
      error_message = e.DetailedDescription();
    }
    // OK to throw here. In the thread that sends repeated keepalives, this exception would be caught,
    // and if an exception is thrown during the initial registration, it is an emergency unless the user
    // decides otherwise.
    CURRENT_THROW(ClaireRegistrationException(service_, route, error_message));
  }

  // The semantic to ensure keepalives only happen from a locked section.
  void SendKeepaliveToKarl(const std::string& route) {
    std::unique_lock<std::mutex> lock(keepalive_mutex_);
    SendKeepaliveToKarl(lock, route);
  }

  // The thread sends periodic keepalive messages.
  void KeepaliveThread() {
    while (!destructing_) {
      std::unique_lock<std::mutex> lock(keepalive_mutex_);

      // Have the interval normalized a bit.
      // TODO(dkorolev): Parameter or named constant for keepalive frequency?
      const std::chrono::microseconds projected_next_keepalive =
          last_keepalive_attempt_result_.timestamp +
          std::chrono::microseconds(current::random::CSRandomUInt64(static_cast<uint64_t>(20e6 * 0.9), static_cast<uint64_t>(20e6 * 1.1)));

      const std::chrono::microseconds now = current::time::Now();

      if (projected_next_keepalive > now) {
        keepalive_condition_variable_.wait_for(
            lock,
            projected_next_keepalive - now,
            [this]() { return destructing_.load() || keepalive_thread_force_wakeup_.load(); });
      }

      if (destructing_) {
        break;
      }

      if (keepalive_thread_force_wakeup_) {
        keepalive_thread_force_wakeup_ = false;
      }

      try {
        SendKeepaliveToKarl(lock, karl_keepalive_route_);
      } catch (const ClaireRegistrationException&) {
        // Ignore exceptions if there's a problem talking to Karl. He'll come back. He's Karl.
      }

      // Generally, unnecessary. For debugging / troubleshooting purposes. -- D.K.
      if (!keepalive_sent_) {
        keepalive_sent_ = true;
        keepalive_condition_variable_.notify_all();
      }
    }

    {
      // Yes, it's an `atomic_bool`, but still lock the mutex to make sure
      // the change from `true` to `false` doesn't interfere with:
      // a) StartKeeepaliveThread, and
      // b) Claire's destructor.
      std::lock_guard<std::mutex> lock(keepalive_thread_running_status_mutex_);
      keepalive_thread_running_ = false;
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
      if (r.url.query.has("initiate_keepalive")) {
        // LCOV_EXCL_START
        ForceSendKeepalive(r.url.query["initiate_keepalive"] == "wait" ? ForceSendKeepaliveWaitRequest::Wait
                                                                       : ForceSendKeepaliveWaitRequest::DoNotWait);
        r("Keepalive will be sent shortly.\n");
        // LCOV_EXCL_STOP
      } else if (r.url.query.has("report_to") && !r.url.query["report_to"].empty()) {
        URL decomposed_karl_url(r.url.query["report_to"]);
        if (!decomposed_karl_url.host.empty()) {
          const std::string karl_url = decomposed_karl_url.ComposeURL();
          SetKarlLocator(Locator(karl_url));
          r("Now reporting to '" + karl_url + "'.\n");
        } else {
          r("Valid URL parameter `report_to` required.\n", HTTPResponseCode.BadRequest);  // LCOV_EXCL_LINE
        }
      } else {
        r("Valid URL parameter required.\n", HTTPResponseCode.BadRequest);  // LCOV_EXCL_LINE
      }
    } else {
      // LCOV_EXCL_START
      r(current::net::DefaultMethodNotAllowedMessage(),
        HTTPResponseCode.MethodNotAllowed,
        net::http::Headers(),
        net::constants::kDefaultHTMLContentType);
      // LCOV_EXCL_STOP
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

  std::atomic_bool destructing_;

  std::atomic_bool keepalive_thread_force_wakeup_;
  mutable std::mutex keepalive_mutex_;
  std::condition_variable keepalive_condition_variable_;
  std::mutex keepalive_thread_running_status_mutex_;
  std::atomic_bool keepalive_thread_running_;
  std::thread keepalive_thread_;

  bool keepalive_sent_ = false;  // For `ForceSendKeepalive(wait == ForceSendKeepaliveWaitRequest::Wait)`. -- D.K.
};

using Claire = GenericClaire<Variant<default_user_status::status>>;

}  // namespace current::karl
}  // namespace current

#endif  // KARL_CLAIRE_H
