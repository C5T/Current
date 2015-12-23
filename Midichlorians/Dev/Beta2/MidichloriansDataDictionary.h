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
// It wraps the inner, platform-independent, event tracking code, and allows client code to stay `.m`.
#error "This C++ header should be `#include`-d or `#import`-ed from an `.mm`, not an `.m` source file."
#endif

#ifndef CURRENT_MIDICHLORIANS_DATA_DICTIONARY_H
#define CURRENT_MIDICHLORIANS_DATA_DICTIONARY_H

#include "../../../TypeSystem/struct.h"

#ifdef COMPILE_MIDICHLORIANS_DATA_DICTIONARY_FOR_IOS_CLIENT
#import <Foundation/Foundation.h>
#import <CoreLocation/CoreLocation.h>
#endif  // COMPILE_MIDICHLORIANS_DATA_DICTIONARY_FOR_IOS_CLIENT

namespace current {
namespace midichlorians {

// Base event.
CURRENT_STRUCT(MidichloriansBaseEvent) {
  CURRENT_FIELD(user_ms, std::chrono::milliseconds);
  CURRENT_FIELD(device_id, std::string);
  CURRENT_FIELD(client_id, std::string);
};

// iOS-specific base event.
CURRENT_STRUCT(iOSBaseEvent, MidichloriansBaseEvent) {
  // TODO: do we need this field?
  CURRENT_FIELD(description, std::string);
};

// iOS event denoting the very first application launch.
// Emitted once during Midichlorians initialization.
CURRENT_STRUCT(iOSFirstLaunchEvent, iOSBaseEvent){};

// iOS event with the detailed application information.
// Emitted once during Midichlorians initialization.
CURRENT_STRUCT(iOSAppLaunchEvent, iOSBaseEvent) {
  CURRENT_FIELD(binary_version, std::string);
  CURRENT_FIELD(cf_version, std::string);
  CURRENT_FIELD(app_install_time, uint64_t);
  CURRENT_FIELD(app_update_time, uint64_t);
  CURRENT_DEFAULT_CONSTRUCTOR(iOSAppLaunchEvent) {}
  CURRENT_CONSTRUCTOR(iOSAppLaunchEvent)(
      const std::string& cf_version, uint64_t app_install_time, uint64_t app_update_time)
      : binary_version(__DATE__ " " __TIME__),
        cf_version(cf_version),
        app_install_time(app_install_time),
        app_update_time(app_update_time) {
    iOSBaseEvent::description = "*FinishLaunchingWithOptions";
  }
};

// iOS event with the detailed device information.
// Emitted once during Midichlorians initialization.
CURRENT_STRUCT(iOSDeviceInfo, iOSBaseEvent) { CURRENT_FIELD(info, (std::map<std::string, std::string>)); };

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
  // TODO: do we need this separation to `fields` + `complex_fields`?
  CURRENT_FIELD(complex_fields, (std::map<std::string, std::string>));
  CURRENT_FIELD(unparsable_fields, std::vector<std::string>);
  CURRENT_DEFAULT_CONSTRUCTOR(iOSGenericEvent) {}
#ifdef COMPILE_MIDICHLORIANS_DATA_DICTIONARY_FOR_IOS_CLIENT
  CURRENT_CONSTRUCTOR(iOSGenericEvent)(NSString * input_event, NSString * input_source, NSDictionary * input) {
    // A somewhat strict yet safe way to parse dictionaries. Imperfect but works. -- D. K.
    event = [input_event UTF8String];
    source = [input_source UTF8String];
    for (NSString* k in input) {
      const char* key = (k && [k isKindOfClass:[NSString class]]) ? [k UTF8String] : nullptr;
      if (key) {
        NSObject* o = [input objectForKey:k];
        NSString* v = (NSString*)o;
        const char* value = (v && [v isKindOfClass:[NSString class]]) ? [v UTF8String] : nullptr;
        if (value) {
          fields[key] = value;
        } else {
          NSString* d = [o description];
          const char* value = (d && [d isKindOfClass:[NSString class]]) ? [d UTF8String] : nullptr;
          if (value) {
            complex_fields[key] = value;
          } else {
            unparsable_fields.push_back(key);
          }
        }
      }
    }
  }
#endif  // COMPILE_MIDICHLORIANS_DATA_DICTIONARY_FOR_IOS_CLIENT
};

using T_IOS_EVENTS = TypeList<iOSFirstLaunchEvent,
                              iOSAppLaunchEvent,
                              iOSDeviceInfo,
                              iOSIdentifyEvent,
                              iOSFocusEvent,
                              iOSGenericEvent>;

}  // namespace midichlorians
}  // namespace current

#endif  // CURRENT_MIDICHLORIANS_DATA_DICTIONARY_H
