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

#include "schema.h"
#include "locator.h"
#include "exceptions.h"

#include "../Blocks/HTTP/api.h"

#include "../Bricks/time/chrono.h"
#include "../Bricks/util/random.h"

namespace current {
namespace karl {

class Claire {
 public:
  using status_filler_t = std::function<void(std::map<std::string, std::string>&)>;

  static std::string GenerateRandomCodename() {
    std::string codename;
    for (int i = 0; i < 6; ++i) {
      codename += static_cast<char>(current::random::CSRandomInt('A', 'Z'));
    }
    return codename;
  }

  Claire(Locator karl, const std::string& service, uint16_t port, status_filler_t status_filler = nullptr)
      : up_(false),
        karl_(karl),
        service_(service),
        codename_(GenerateRandomCodename()),
        port_(port),
        status_filler_(status_filler),
        us_start_(current::time::Now()),
        http_scope_(HTTP(port).Register("/.current",
                                        [this](Request r) {
                                          if (!up_) {
                                            ClaireToKarlBase response;
                                            FillBase(response);
                                            r(response);
                                          } else {
                                            const auto now = current::time::Now();
                                            ClaireToKarl response;
                                            FillBase(response);
                                            response.us_start = us_start_;
                                            response.us_now = now;
                                            response.us_uptime = now - us_start_;
                                            if (status_filler_) {
                                              status_filler_(response.status);
                                            }
                                            r(response);
                                          }
                                        })) {}
  void Register() {
    // Register self with Karl.
    // During this call, Karl would crawl the endpoint of this service, and, if everything is successful,
    // register this service as the running and browsable one.
    const std::string route =
        karl_.address_port_route + "?codename=" + codename_ + "&port=" + current::ToString(port_);
    try {
      if (HTTP(POST(route, "")).code == HTTPResponseCode.OK) {
        up_ = true;
      } else {
        CURRENT_THROW(ClaireRegistrationException(service_, route));
      }
    } catch (const current::Exception&) {
      CURRENT_THROW(ClaireRegistrationException(service_, route));
    }
  }

 private:
  void FillBase(ClaireToKarlBase& response) const {
    response.up = up_;
    response.codename = codename_;
    response.service = service_;
    response.local_port = port_;
  }

  Claire() = delete;

  std::atomic_bool up_;
  const Locator karl_;
  const std::string service_;
  const std::string codename_;
  const int port_;
  const status_filler_t status_filler_;
  const std::chrono::microseconds us_start_;
  const HTTPRoutesScope http_scope_;
};

}  // namespace current::karl
}  // namespace current

#endif  // KARL_CLAIRE_H
