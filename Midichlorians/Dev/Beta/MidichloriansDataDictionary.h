/*******************************************************************************
 The MIT License (MIT)

 Copyright (c) 2015:

 * Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
 * Alexander Zolotarev <me@alex.bio> from Minsk, Belarus

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

#include <string>

#include "../../../Bricks/cerealize/cerealize.h"

#ifdef COMPILE_MIDICHLORIANS_DATA_DICTIONARY_FOR_IOS_CLIENT
#import <Foundation/Foundation.h>
#import <CoreLocation/CoreLocation.h>
#endif  // COMPILE_MIDICHLORIANS_DATA_DICTIONARY_FOR_IOS_CLIENT

struct MidichloriansEvent {
  mutable std::string device_id;
  mutable std::string client_id;
  void SetDeviceId(const std::string& did) const { device_id = did; }
  void SetClientId(const std::string& cid) const { client_id = cid; }

  template <class A>
  void serialize(A& ar) {
    ar(CEREAL_NVP(device_id), CEREAL_NVP(client_id));
  }

  virtual std::string EventAsString(const std::string& device_id, const std::string& client_id) const {
    SetDeviceId(device_id);
    SetClientId(client_id);
    return CerealizeJSON(WithBaseType<MidichloriansEvent>(*this));
  }
};

// A helper macro to define structured events.

#ifdef CURRENT_EVENT
#error "The `CURRENT_EVENT` macro should not be defined."
#endif

#define CURRENT_EVENT(M_EVENT_CLASS_NAME, M_IMMEDIATE_BASE)                                            \
  struct M_EVENT_CLASS_NAME;                                                                           \
  CEREAL_REGISTER_TYPE(M_EVENT_CLASS_NAME);                                                            \
  struct M_EVENT_CLASS_NAME##Helper : M_IMMEDIATE_BASE {                                               \
    typedef MidichloriansEvent CEREAL_BASE_TYPE;                                                       \
    typedef M_IMMEDIATE_BASE SUPER;                                                                    \
    virtual std::string EventAsString(const std::string& did, const std::string& cid) const override { \
      SetDeviceId(did);                                                                                \
      SetClientId(cid);                                                                                \
      return CerealizeJSON(WithBaseType<MidichloriansEvent>(*this));                                   \
    }                                                                                                  \
    template <class A>                                                                                 \
    void serialize(A& ar) {                                                                            \
      SUPER::serialize(ar);                                                                            \
    }                                                                                                  \
  };                                                                                                   \
  struct M_EVENT_CLASS_NAME : M_EVENT_CLASS_NAME##Helper

// Generic iOS events.

CURRENT_EVENT(iOSBaseEvent, MidichloriansEvent) {
  std::string description;
  template <typename A>
  void serialize(A & ar) {
    SUPER::serialize(ar);
    ar(CEREAL_NVP(description));
  }
  iOSBaseEvent() = default;
  iOSBaseEvent(const std::string& d) : description(d) {}
};

CURRENT_EVENT(iOSIdentifyEvent, iOSBaseEvent) {
  // An empty event, to catch the timestamp of the `identify()` call.
  template <typename A>
  void serialize(A & ar) {
    SUPER::serialize(ar);
  }
  iOSIdentifyEvent() = default;
};

CURRENT_EVENT(iOSDeviceInfo, iOSBaseEvent) {
  std::map<std::string, std::string> info;
  template <typename A>
  void serialize(A & ar) {
    SUPER::serialize(ar);
    ar(CEREAL_NVP(info));
  }
  iOSDeviceInfo() = default;
  iOSDeviceInfo(const std::map<std::string, std::string>& info) : info(info) { description = "iPhoneInfo"; }
};

CURRENT_EVENT(iOSAppLaunchEvent, iOSBaseEvent) {
  std::string binary_version;
  std::string cf_version;
  uint64_t app_install_time;
  uint64_t app_update_time;
  template <typename A>
  void serialize(A & ar) {
    SUPER::serialize(ar);
    ar(CEREAL_NVP(binary_version),
       CEREAL_NVP(cf_version),
       CEREAL_NVP(app_install_time),
       CEREAL_NVP(app_update_time));
  }
  iOSAppLaunchEvent() = default;
  iOSAppLaunchEvent(const std::string& cf_version, uint64_t app_install_time, uint64_t app_update_time)
      : binary_version(__DATE__ " " __TIME__),
        cf_version(cf_version),
        app_install_time(app_install_time),
        app_update_time(app_update_time) {
    description = "*FinishLaunchingWithOptions";
  }
};

CURRENT_EVENT(iOSFirstLaunchEvent, iOSBaseEvent) {
  template <typename A>
  void serialize(A & ar) {
    SUPER::serialize(ar);
  }
  iOSFirstLaunchEvent() = default;
};

// TODO(dk+mz): `source` argument goes to `description` for `iOSFocusEvent`,
// while for `iOSGenericEvent` there's dedicated member `source`. Fix?
CURRENT_EVENT(iOSFocusEvent, iOSBaseEvent) {
  bool gained_focus;
  template <typename A>
  void serialize(A & ar) {
    SUPER::serialize(ar);
    ar(CEREAL_NVP(gained_focus));
  }
  iOSFocusEvent() = default;
  iOSFocusEvent(bool a, const std::string& d = "") {
    description = d;
    gained_focus = a;
  }
};

CURRENT_EVENT(iOSGenericEvent, iOSBaseEvent) {
  std::string event;
  std::string source;
  std::map<std::string, std::string> fields;
  std::map<std::string, std::string> complex_fields;
  std::set<std::string> unparsable_fields;
  template <typename A>
  void serialize(A & ar) {
    SUPER::serialize(ar);
    ar(CEREAL_NVP(event),
       CEREAL_NVP(source),
       CEREAL_NVP(fields),
       CEREAL_NVP(complex_fields),
       CEREAL_NVP(unparsable_fields));
  }
  iOSGenericEvent() = default;
  iOSGenericEvent(const std::map<std::string, std::string>& dictionary) : fields(dictionary) {}

#ifdef COMPILE_MIDICHLORIANS_DATA_DICTIONARY_FOR_IOS_CLIENT
  iOSGenericEvent(NSString * input_event, NSString * input_source, NSDictionary * input) {
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
            unparsable_fields.insert(key);
          }
        }
      }
    }
  }
#endif  // COMPILE_MIDICHLORIANS_DATA_DICTIONARY_FOR_IOS_CLIENT
};

#endif  // CURRENT_MIDICHLORIANS_DATA_DICTIONARY_H
