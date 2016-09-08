/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef __cplusplus
// If you get this error building an Objective-C project, #include neither this file nor "MidichloriansImpl.h".
// Use "Midichlorians.h" instead.
// It wraps the inner, platform-independent, event tracking code, and allows the client code to stay `.m`.
#error "This C++ header should be `#include`-d or `#import`-ed from an `.mm`, not an `.m` source file."
#endif

#ifndef CURRENT_MIDICHLORIANS_DATA_DICTIONARY_H
#define CURRENT_MIDICHLORIANS_DATA_DICTIONARY_H

#include "../port.h"

#include <chrono>

#include "../TypeSystem/struct.h"

#ifdef COMPILE_MIDICHLORIANS_DATA_DICTIONARY_FOR_IOS_CLIENT
#import <Foundation/Foundation.h>
#import <CoreLocation/CoreLocation.h>
#endif  // COMPILE_MIDICHLORIANS_DATA_DICTIONARY_FOR_IOS_CLIENT

namespace current {
namespace midichlorians {

// `MidichloriansBaseEvent` is the universal base class for all analytics events, mobile and web.
// `customer_id` is used to separate events from different customers, when they are collected by a single
// server, and could be ignored in case of single-customer environment.
// TODO(dk+mz): make `customer_id` a mandatory parameter in initialization?
// `user_ms` is the event timestamp on user device, measured in milliseconds since epoch.
// `client_id` is the unique identifier of the user. It could be set by corresponding `identify` methods or
// generated automatically.
CURRENT_STRUCT(MidichloriansBaseEvent) {
  CURRENT_FIELD(customer_id, std::string);
  CURRENT_FIELD(user_ms, std::chrono::milliseconds);
  CURRENT_FIELD(client_id, std::string);
};

namespace ios {

CURRENT_STRUCT(iOSBaseEvent, MidichloriansBaseEvent) { CURRENT_FIELD(device_id, std::string); };

// iOS event denoting the very first application launch.
// Emitted once during Midichlorians initialization.
CURRENT_STRUCT(iOSFirstLaunchEvent, iOSBaseEvent){};

// iOS event with the detailed application information.
// Emitted once during Midichlorians initialization.
CURRENT_STRUCT(iOSAppLaunchEvent, iOSBaseEvent) {
  CURRENT_FIELD(binary_version, std::string);
  CURRENT_FIELD(cf_version, std::string);
  CURRENT_FIELD(app_install_time, uint64_t, 0ull);
  CURRENT_FIELD(app_update_time, uint64_t, 0ull);
  CURRENT_DEFAULT_CONSTRUCTOR(iOSAppLaunchEvent) {}
  CURRENT_CONSTRUCTOR(iOSAppLaunchEvent)(
      const std::string& cf_version, uint64_t app_install_time, uint64_t app_update_time)
      : binary_version(__DATE__ " " __TIME__),
        cf_version(cf_version),
        app_install_time(app_install_time),
        app_update_time(app_update_time) {}
};

// iOS event with the detailed device information.
// Emitted once during Midichlorians initialization.
CURRENT_STRUCT(iOSDeviceInfo, iOSBaseEvent) {
  CURRENT_FIELD(info, (std::map<std::string, std::string>));
  CURRENT_DEFAULT_CONSTRUCTOR(iOSDeviceInfo) {}
  CURRENT_CONSTRUCTOR(iOSDeviceInfo)(const std::map<std::string, std::string>& info) : info(info) {}
};

// iOS event emitted by `identify` method.
CURRENT_STRUCT(iOSIdentifyEvent, iOSBaseEvent){};

// iOS event emitted by `focusEvent` method.
CURRENT_STRUCT(iOSFocusEvent, iOSBaseEvent) {
  CURRENT_FIELD(gained_focus, bool);
  CURRENT_FIELD(source, std::string);
  CURRENT_CONSTRUCTOR(iOSFocusEvent)(bool gained_focus = false, const std::string& source = "")
      : gained_focus(gained_focus), source(source) {}
};

CURRENT_STRUCT(iOSGenericEvent, iOSBaseEvent) {
  CURRENT_FIELD(event, std::string);
  CURRENT_FIELD(source, std::string);
  CURRENT_FIELD(fields, (std::map<std::string, std::string>));
  CURRENT_FIELD(unparsable_fields, std::vector<std::string>);
  CURRENT_DEFAULT_CONSTRUCTOR(iOSGenericEvent) {}  // LCOV_EXCL_LINE
#ifdef COMPILE_MIDICHLORIANS_DATA_DICTIONARY_FOR_IOS_CLIENT
  CURRENT_CONSTRUCTOR(iOSGenericEvent)(NSString * event_name, NSString * event_source, NSDictionary * properties) {
    // A somewhat strict yet safe way to parse dictionaries. Imperfect but works. -- D. K.
    event = [event_name UTF8String];
    source = [event_source UTF8String];
    for (NSString* k in properties) {
      const char* key = (k && [k isKindOfClass:[NSString class]]) ? [k UTF8String] : nullptr;
      if (key) {
        NSObject* o = [properties objectForKey:k];
        NSString* v = (NSString*)o;
        const char* value = (v && [v isKindOfClass:[NSString class]]) ? [v UTF8String] : nullptr;
        if (value) {
          fields[key] = value;
        } else {
          NSString* d = [o description];
          const char* value = (d && [d isKindOfClass:[NSString class]]) ? [d UTF8String] : nullptr;
          if (value) {
            fields[key] = value;
          } else {
            unparsable_fields.push_back(key);
          }
        }
      }
    }
  }
#endif  // COMPILE_MIDICHLORIANS_DATA_DICTIONARY_FOR_IOS_CLIENT
};

using ios_events_t =
    TypeList<iOSFirstLaunchEvent, iOSAppLaunchEvent, iOSDeviceInfo, iOSIdentifyEvent, iOSFocusEvent, iOSGenericEvent>;

}  // namespace ios

namespace web {

//  Event collection server expects the following HTTP headers to be set:
//   `X-Forwarded-For` for `ip`,
//   `User-Agent` for `user_agent`,
//   `Referer` for `referer_*` members.
//
// TODO(mzhirovich): sync up with @sompylasar on the proper structure of web events.
// TODO(dkorolev): add sample nginx config + link to it in the comments.

CURRENT_STRUCT(WebBaseEvent, MidichloriansBaseEvent) {
  CURRENT_FIELD(ip, std::string);
  CURRENT_FIELD(user_agent, std::string);
  CURRENT_FIELD(referer_host, std::string);
  CURRENT_FIELD(referer_path, std::string);
  CURRENT_FIELD(referer_querystring, (std::map<std::string, std::string>));
};

CURRENT_STRUCT(WebEnterEvent, WebBaseEvent){};

CURRENT_STRUCT(WebExitEvent, WebBaseEvent){};

CURRENT_STRUCT(WebForegroundEvent, WebBaseEvent){};

CURRENT_STRUCT(WebBackgroundEvent, WebBaseEvent){};

CURRENT_STRUCT(WebGenericEvent, WebBaseEvent) {
  CURRENT_FIELD(event_category, std::string);
  CURRENT_FIELD(event_action, std::string);
  CURRENT_DEFAULT_CONSTRUCTOR(WebGenericEvent) {}
  CURRENT_CONSTRUCTOR(WebGenericEvent)(const std::string& category, const std::string& action)
      : event_category(category), event_action(action) {}
};

using web_events_t = TypeList<WebEnterEvent, WebExitEvent, WebForegroundEvent, WebBackgroundEvent, WebGenericEvent>;

}  // namespace web

}  // namespace midichlorians
}  // namespace current

#endif  // CURRENT_MIDICHLORIANS_DATA_DICTIONARY_H
