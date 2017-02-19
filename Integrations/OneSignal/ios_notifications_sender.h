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

#ifndef INTEGRATIONS_ONESIGNAL_IOS_NOTIFICATIONS_SENDER_H
#define INTEGRATIONS_ONESIGNAL_IOS_NOTIFICATIONS_SENDER_H

#include "exceptions.h"

#include "../../Blocks/HTTP/api.h"

#include "../../TypeSystem/struct.h"
#include "../../TypeSystem/Serialization/json.h"

namespace current {
namespace integrations {
namespace onesignal {

// The text of banner or alert notification.
CURRENT_STRUCT(OneSignalNotificationRequestMessage) {
  CURRENT_FIELD(en, std::string);
  CURRENT_CONSTRUCTOR(OneSignalNotificationRequestMessage)(const std::string& message = "") : en(message) {}
};

CURRENT_STRUCT(OneSignalNotificationRequest) {
  // App ID, required.
  CURRENT_FIELD(app_id, std::string);

  // Required to eliminate room for accidental spam.
  CURRENT_FIELD(include_player_ids, std::vector<std::string>);

  // Contents, set for banner or alert notification.
  CURRENT_FIELD(contents, Optional<OneSignalNotificationRequestMessage>);

  // Badge increase, set for App Icon, the number in a red circle on the launcher screen.
  CURRENT_FIELD(ios_badgeType, Optional<std::string>);  // "Increase"
  CURRENT_FIELD(ios_badgeCount, Optional<int32_t>);     // An integer above zero.

  // If sending a badge increase notification only, with no `contents`, `content_available` must be `true`.
  CURRENT_FIELD(content_available, Optional<bool>);
};

CURRENT_STRUCT(OneSignalNotificationResponse) {
  CURRENT_FIELD(id, Optional<std::string>);
  CURRENT_FIELD(recipients, Optional<int64_t>);
  CURRENT_FIELD(errors, Optional<std::vector<std::string>>);
};

constexpr static int16_t kDefaultOneSignalIntegrationPort = 24001;

class IOSPushNotificationsSender final {
 public:
  IOSPushNotificationsSender(const std::string& app_id, uint16_t port = kDefaultOneSignalIntegrationPort)
      : app_id_(app_id), post_url_(strings::Printf("localhost:%d", port)) {
#ifndef CURRENT_CI
    const std::string url = strings::Printf("localhost:%d/.current", port);
    const std::string golden = "https://onesignal.com/api/v1/notifications\n";
    std::string actual;
    try {
      actual = HTTP(GET(url)).body;
    } catch (const Exception&) {
      throw OneSignalInitializationException(strings::Printf(
          "OneSignal Integration HTTP request failed on: %s\nLikely misconfigured nginx.", url.c_str()));
// Here is the golden nginx config if you need one.
#if 0
server {
  listen 24001;
  location /.current {
    return 200 'https://onesignal.com/api/v1/notifications\n';
  }
  location / {
    proxy_pass https://onesignal.com/api/v1/notifications;
    proxy_pass_request_headers on;
  }
}
#endif
    }
    if (actual != golden) {
      throw OneSignalInitializationException(strings::Printf(
          "OneSignal Integration HTTP request to `%s` returned an unexpected result.\nExpected: %s\nActual:  "
          "%s\n",
          url.c_str(),
          golden.c_str(),
          actual.c_str()));
    }
#endif
  }

  // Send one iOS push notification.
  // Returns true on success.
  // Returns false on "regular" errors.
  // Throws on terrible errors, with the returned `e.DetailedDescription()` good for journaling and further
  // investigation.
  bool Push(const std::string& recipient_player_id,
            const std::string& message = "",
            int32_t increase_counter = 0) const {
#ifdef CURRENT_CI
    static_cast<void>(recipient_player_id);
    static_cast<void>(message);
    static_cast<void>(increase_counter);
    return true;
#else
    if (message.empty() && !increase_counter) {
      return true;
    } else {
      OneSignalNotificationRequest request;

      request.app_id = app_id_;
      request.include_player_ids.push_back(recipient_player_id);

      if (!message.empty()) {
        request.contents = OneSignalNotificationRequestMessage(message);
      } else {
        request.content_available = true;
      }

      if (increase_counter) {
        request.ios_badgeType = "Increase";
        request.ios_badgeCount = increase_counter;
      }

      try {
        const auto response = HTTP(POST(post_url_, JSON<JSONFormat::Minimalistic>(request))
                                       .SetHeader("Content-Type", net::constants::kDefaultJSONContentType));
        OneSignalNotificationResponse parsed_response;
        try {
          ParseJSON(response.body, parsed_response);
        } catch (const Exception& e) {
          // Resulting JSON error: Fatal.
          throw OneSignalPushNotificationException("OneSignal iOS push error A: " + e.DetailedDescription());
        }
        if (!Exists(parsed_response.recipients)) {
          if (Exists(parsed_response.errors)) {
            // Some "regular" error occurred: Non-fatal.
            return false;
          } else {
            // Some unexpected error occurred: Fatal.
            throw OneSignalPushNotificationException("OneSignal iOS push error Q: " + response.body);
          }
        }
        if (Value(parsed_response.recipients) == 0) {
          // Just could not send the notification: Non-fatal.
          return false;
        } else if (Value(parsed_response.recipients) > 1) {
          // Potentially sent the notification to more than one user: Fatal.
          throw OneSignalPushNotificationException("OneSignal iOS push error Z: " + response.body);
        } else {
          // The notification has been sent to exactly one user: OK.
          return true;
        }
      } catch (const Exception& e) {
        // HTTP exception, not fatal.
        return false;
      }
    }
#endif  // CURRENT_CI
  }

  bool Push(const std::string& recipient_player_id, int32_t increase_counter) const {
    return Push(recipient_player_id, "", increase_counter);
  }

 private:
  const std::string app_id_;
  const std::string post_url_;
};

}  // namespace current::integrations::onesignal
}  // namespace current::integrations
}  // namespace current

#endif  // INTEGRATIONS_ONESIGNAL_IOS_NOTIFICATIONS_SENDER_H
