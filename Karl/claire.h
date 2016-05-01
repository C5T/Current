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
#include "../Bricks/net/http/impl/server.h"  // current::net::constants::kDefaultJSONContentType

namespace current {
namespace karl {

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
                                          if (!all && build) {
                                            r(build::Info());
                                          } else {
                                            r(JSON<JSONFormat::Minimalistic>(GenerateKeepaliveStatus(all)),
                                              HTTPResponseCode.OK,
                                              current::net::constants::kDefaultJSONContentType);
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
        std::lock_guard<std::mutex> lock(status_generator_mutex_);
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
#ifndef CURRENT_MOCK_TIME
    const auto now = current::time::Now();
    assert(now >= us_start_);
#else
    const auto now = us_start_;  // To avoid negatives in `status.uptime`, which would kill `ParseJSON`. -- D.K.
#endif

    status.service = service_;
    status.codename = codename_;
    status.local_port = port_;

    status.now = now;
    status.uptime = now - us_start_;
    status.uptime_as_string = current::strings::TimeIntervalAsHumanReadableString(status.uptime);

    if (fill_current_build) {
      status.build = build::Info();
    }
  }

  void FillKeepaliveStatus(specific_status_t& status, bool fill_current_build = true) const {
    FillBaseKeepaliveStatus(status, fill_current_build);
    {
      std::lock_guard<std::mutex> lock(status_generator_mutex_);
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
      if (HTTP(POST(route,
                    JSON<JSONFormat::Minimalistic>(keepalive_body),
                    current::net::constants::kDefaultJSONContentType)).code == HTTPResponseCode.OK) {
        return;
      }
    } catch (const current::Exception&) {
    }
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

      // TODO(dkorolev): Parameter or named constant for keepalive frequency?
      keepalive_condition_variable_.wait_for(lock, std::chrono::seconds(20));

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

  mutable std::mutex status_generator_mutex_;
  status_generator_t status_generator_;

  const std::chrono::microseconds us_start_;
  const HTTPRoutesScope http_scope_;

  std::atomic_bool keepalive_thread_terminating_;
  std::mutex keepalive_mutex_;
  std::condition_variable keepalive_condition_variable_;
  std::thread keepalive_thread_;
};

using Claire = GenericClaire<DefaultClaireServiceStatus>;

}  // namespace current::karl
}  // namespace current

#endif  // KARL_CLAIRE_H
