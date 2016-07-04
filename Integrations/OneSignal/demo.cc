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

#include "ios_notifications_sender.h"

#include "../../Bricks/dflags/dflags.h"

DEFINE_string(app_id, "", "The `app_id` for OneSignal integration.");
DEFINE_uint16(port, 24001, "The port from which the local nginx proxies request to OneSignal.");

DEFINE_string(recipient_id, "", "The `recipient_id` for OneSignal integration.");
DEFINE_string(message, "", "The string to send as the message.");
DEFINE_int32(increase, 0, "By how much should the badge icon counter increase.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  if (FLAGS_app_id.empty()) {
    std::cerr << "The `--app_id` flag should be set." << std::endl;
    return -1;
  }

  current::integrations::onesignal::IOSPushNotificationsSender client(FLAGS_app_id, FLAGS_port);

  if (FLAGS_recipient_id.empty()) {
    std::cerr << "The `--recipient_id` flag should be set." << std::endl;
    return -1;
  }

  if (FLAGS_message.empty() && FLAGS_increase == 0) {
    std::cerr << "At least one of the `--message` and `--increase` flags should be set." << std::endl;
    return -1;
  } else {
    try {
      if (client.Push(FLAGS_recipient_id, FLAGS_message, FLAGS_increase)) {
        return 0;
      } else {
        std::cerr << "Failed." << std::endl;
        return 1;
      }
    } catch (const current::Exception& e) {
      std::cerr << "Failed: " << e.What() << std::endl;
      return -1;
    }
  }
}
