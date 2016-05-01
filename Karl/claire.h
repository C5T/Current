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

// NOTE: Local `current_build.h` must be included before Karl/Claire headers.

#ifndef KARL_CLAIRE_H
#define KARL_CLAIRE_H

#include "../port.h"

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "schema.h"
#include "locator.h"
#include "exceptions.h"

#include "../Blocks/HTTP/api.h"

#include "../Bricks/time/chrono.h"
#include "../Bricks/util/random.h"

namespace current {
namespace karl {

inline std::string KeepaliveAttemptResultAsString(const KeepaliveAttemptResult& result) {
  if (result.status == KeepaliveAttemptStatus::Unknown) {
    return "Unknown";
  } else if (result.status == KeepaliveAttemptStatus::Success) {
    return "Success";
  } else if (result.status == KeepaliveAttemptStatus::CouldNotConnect) {
    return "HTTP connection attempt failed";
  } else if (result.status == KeepaliveAttemptStatus::ErrorCodeReturned) {
    return "HTTP response code " + current::ToString(result.http_code);
  } else {
    return "";
  }
}

template <class T>
class GenericClaire final {
 public:
  using specific_status_t = ClaireServiceStatus<T>;
  using status_generator_t = std::function<T()>;

  static std::string GenerateRandomCodename() {
    std::string codename;
    for (int i = 0; i < 6; ++i) {
      codename += static_cast<char>(current::random::CSRandomInt('A', 'Z'));
    }
    return codename;
  }

  GenericClaire(Locator karl, const std::string& service, uint16_t port)
      : in_beacon_mode_(false),
        karl_(karl),
        service_(service),
        codename_(GenerateRandomCodename()),
        port_(port),
        karl_keepalive_route_(karl_.address_port_route + "?codename=" + codename_ + "&port=" +
                              current::ToString(port_)),
        us_start_(current::time::Now()),
        http_scope_(HTTP(port).Register("/.current",
                                        [this](Request r) {
                                          const auto& qs = r.url.query;
                                          const bool all = qs.has("all") || qs.has("a");
                                          const bool build = qs.has("build") || qs.has("b");
                                          const bool runtime = qs.has("runtime") || qs.has("r");
                                          if (!all && build) {
                                            r(build::Info());
                                          } else if (!all && runtime) {
                                            r(([this]() -> Response {
                                              std::lock_guard<std::mutex> lock(status_mutex_);
                                              if (status_generator_) {
                                                return Response(status_generator_());
                                              } else {
                                                return Response("Not ready.\n",
                                                                HTTPResponseCode.ServiceUnavailable);
                                              }
                                            })());
                                          } else {
                                            // Don't use `JSONFormat::Minimalistic` to support type evolution
                                            // of how to report/aggregate/render statuses on the Karl side.
                                            r(GenerateKeepaliveStatus(all));
                                          }
                                        })),
        keepalive_thread_terminating_(false) {}

  virtual ~GenericClaire() {
    if (keepalive_thread_.joinable()) {
      keepalive_thread_terminating_ = true;
      keepalive_condition_variable_.notify_one();
      keepalive_thread_.join();
    }
  }

  void Register(status_generator_t status_filler = nullptr, bool require_karls_confirmation = false) {
    // Register this Claire with Karl and spawn the thread to send regular keepalives.
    // If `require_karls_confirmation` is true, throw if Karl can be not be reached.
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

 private:
  void FillBaseKeepaliveStatus(ClaireStatus& status, bool fill_current_build = true) const {
    const auto now = current::time::Now();

#ifndef CURRENT_MOCK_TIME
    // With mock time, can result in trouble. -- D.K.
    assert(now >= us_start_);
#endif

    status.service = service_;
    status.codename = codename_;
    status.local_port = port_;

    status.now = now;

#ifndef CURRENT_MOCK_TIME
    // With mock time, can result in negatives in `status.uptime`, which would kill `ParseJSON`. -- D.K.
    status.uptime_epoch_microseconds = now - us_start_;
    status.uptime = current::strings::TimeIntervalAsHumanReadableString(status.uptime_epoch_microseconds);

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
        status.last_successful_keepalive_ping =
            current::strings::Printf("%.2lfms", 1e-3 * last_successful_keepalive_ping_.count());
      }
    }
#endif

    if (fill_current_build) {
      status.build = build::Info();
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
    specific_status_t status;
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
    // TODO(dk+mz): Should it really throw in keepalive thread? It will crash the binary.
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

  GenericClaire() = delete;

  std::atomic_bool in_beacon_mode_;

  const Locator karl_;
  const std::string service_;
  const std::string codename_;
  const int port_;
  const std::string karl_keepalive_route_;

  const std::chrono::microseconds us_start_;

  mutable std::mutex status_mutex_;
  status_generator_t status_generator_;
  KeepaliveAttemptResult last_keepalive_attempt_result_;
  std::chrono::microseconds last_successful_keepalive_timestamp_ = std::chrono::microseconds(0);
  std::chrono::microseconds last_successful_keepalive_ping_ = std::chrono::microseconds(0);

  const HTTPRoutesScope http_scope_;

  std::atomic_bool keepalive_thread_terminating_;
  std::mutex keepalive_mutex_;
  std::condition_variable keepalive_condition_variable_;
  std::thread keepalive_thread_;
};

using Claire = GenericClaire<Variant<DefaultClaireServiceStatus>>;

}  // namespace current::karl
}  // namespace current

#endif  // KARL_CLAIRE_H
