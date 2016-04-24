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

#include "schema.h"
#include "locator.h"
#include "exceptions.h"

#include "../Blocks/HTTP/api.h"

#include "../Bricks/time/chrono.h"
#include "../Bricks/util/random.h"

namespace current {
namespace karl {

template <class STATUS>
class GenericClaire final {
 public:
  using status_t = STATUS;
  using status_filler_t = std::function<void(status_t&)>;

  static std::string GenerateRandomCodename() {
    std::string codename;
    for (int i = 0; i < 6; ++i) {
      codename += static_cast<char>(current::random::CSRandomInt('A', 'Z'));
    }
    return codename;
  }

  GenericClaire(Locator karl, const std::string& service, uint16_t port)
      : register_called_(false),
        registered_(false),
        karl_(karl),
        service_(service),
        codename_(GenerateRandomCodename()),
        port_(port),
        us_start_(current::time::Now()),
        http_scope_(HTTP(port).Register("/.current",
                                        [this](Request r) {
                                          if (r.url.query.has("build")) {
                                            r(build::Info());
                                          } else {
                                            if (!registered_) {
                                              ClaireStatusBase response;
                                              FillBase(response, r.url.query.has("all"));
                                              r(response);
                                            } else {
                                              status_t response;
                                              FillBase(response, r.url.query.has("all"));
                                              if (status_filler_) {
                                                std::lock_guard<std::mutex> lock(mutex_);
                                                status_filler_(response);
                                              }
                                              r(response);
                                            }
                                          }
                                        })) {
    // TODO(dkorolev) + TODO(mzhurovich): Should Claire at least check whether Karl is available at
    // construction?
  }

  ~GenericClaire() {
    if (keepalive_thread_.joinable()) {
      keepalive_thread_.join();
    }
  }

  void Register(status_filler_t status_filler = nullptr, bool strict = false) {
    // Register this Claire with Karl and spawn the thread to send regular keepalives.
    // If `strict` is true, throw if Karl can be not be reached.
    // If `strict` is false, just start the keepalives thread.
    std::lock_guard<std::mutex> lock(mutex_);
    if (!register_called_) {
      register_called_ = true;
      status_filler_ = status_filler;
      // Only register once.
      if (strict) {
        // During this call, Karl would crawl the endpoint of this service, and, if everything is successful,
        // register this service as the running and browsable one.
        const std::string route =
            karl_.address_port_route + "?codename=" + codename_ + "&port=" + current::ToString(port_);
        try {
          if (HTTP(POST(route, "")).code == HTTPResponseCode.OK) {
            registered_ = true;
          } else {
            CURRENT_THROW(ClaireRegistrationException(service_, route));
          }
        } catch (const current::Exception&) {
          CURRENT_THROW(ClaireRegistrationException(service_, route));
        }
      } else {
        // In non-`strict` more, simply mark the service as `registered`.
        registered_ = true;
      }

      keepalive_thread_ = std::thread([this]() { Thread(); });
    }
  }

 private:
  void FillBase(ClaireStatusBase& status, bool fill_build) const {
    status.service = service_;
    status.codename = codename_;
    status.local_port = port_;

    status.registered = registered_;

    status.us_start = us_start_;
    status.us_now = current::time::Now();

    if (fill_build) {
      status.build = build::Info();
    }
  }

  void Thread() {}

  GenericClaire() = delete;

  bool register_called_;
  std::atomic_bool registered_;
  std::mutex mutex_;

  const Locator karl_;
  const std::string service_;
  const std::string codename_;
  const int port_;
  status_filler_t status_filler_;
  const std::chrono::microseconds us_start_;
  const HTTPRoutesScope http_scope_;

  std::thread keepalive_thread_;
};

using Claire = GenericClaire<ClaireStatus>;

}  // namespace current::karl
}  // namespace current

#endif  // KARL_CLAIRE_H
