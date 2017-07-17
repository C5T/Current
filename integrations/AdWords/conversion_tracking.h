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

#ifndef INTEGRATIONS_ADWORDS_CONVERSION_TRACKING_H
#define INTEGRATIONS_ADWORDS_CONVERSION_TRACKING_H

#include "exceptions.h"

#include "../../blocks/HTTP/api.h"

#include "../../typesystem/struct.h"
#include "../../typesystem/Serialization/json.h"

namespace current {
namespace integrations {
namespace adwords {
namespace conversion_tracking {

CURRENT_STRUCT(AdWordsConversionTrackingParams) {
  CURRENT_FIELD(conversion_id, std::string);
  CURRENT_FIELD(label, std::string);
  CURRENT_FIELD(bundleid, std::string);
  CURRENT_FIELD(idtype, std::string);
};

constexpr static int16_t kDefaultAdWordsIntegrationPort = 24002;

class AdWordsMobileConversionEventsSender final {
 public:
  AdWordsMobileConversionEventsSender(const AdWordsConversionTrackingParams& params,
                                      uint16_t port = kDefaultAdWordsIntegrationPort)
      : get_url_(
            strings::Printf("localhost:%d/googleadservices/pagead/conversion/%s/?label=%s&bundleid=%s&idtype=%s&rdid=",
                            port,
                            params.conversion_id.c_str(),
                            params.label.c_str(),
                            params.bundleid.c_str(),
                            params.idtype.c_str())) {
#ifndef CURRENT_CI
    const std::string url = strings::Printf("localhost:%d/.current/googleadservices", port);
    const std::string golden = "https://www.googleadservices.com/\n";
    std::string actual;
    try {
      actual = HTTP(GET(url)).body;
    } catch (const Exception&) {
      CURRENT_THROW(AdWordsInitializationException(
          strings::Printf("AdWords Integration HTTP request failed on: %s\nLikely misconfigured nginx.", url.c_str())));
// Here is the golden nginx config if you need one.
#if 0
server {
  listen 24002;
  location /.current/googleadservices {
    return 200 'https://www.googleadservices.com/\n';
  }
  location /googleadservices/ {
    proxy_pass https://www.googleadservices.com/;
    proxy_pass_request_headers on;
  }
  location /.current {
    return 200 'Need parameter.\n';
  }
}
#endif
    }
    if (actual != golden) {
      CURRENT_THROW(AdWordsInitializationException(strings::Printf(
          "AdWords Integration HTTP request to `%s` returned an unexpected result.\nExpected: %s\nActual:  "
          "%s\n",
          url.c_str(),
          golden.c_str(),
          actual.c_str())));
    }
#endif  // CURRENT_CI
  }

  // Sends one conversion event. Returns `true` on success.
  // The `rdid` parameter is the advertising ID on iOS.
  // Doc: https://developers.google.com/app-conversion-tracking/ios/conversion-tracking-server
  bool SendConversionEvent(const std::string& rdid) const {
#ifdef CURRENT_CI
    static_cast<void>(rdid);
    return true;
#else
    // TODO: Look into "instant confirmation of app conversions" (*), where 302 is OK too.
    // (*) https://developers.google.com/app-conversion-tracking/ios/install-confirm)
    const auto response = HTTP(GET(get_url_ + rdid));
    return response.code == HTTPResponseCode.OK;
#endif  // CURRENT_CI
  }

 private:
  const std::string get_url_;
};

}  // namespace current::integrations::adwords::conversion_tracking
}  // namespace current::integrations::adwords
}  // namespace current::integrations
}  // namespace current

#endif  // INTEGRATIONS_ADWORDS_CONVERSION_TRACKING_H
