/*******************************************************************************
 The MIT License (MIT)

 Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
           (c) 2015 Alexander Zolotarev <me@alex.bio> from Minsk, Belarus
           (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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
// If you get this error building an Objective-C project, #include neither this file
// nor "MidichloriansDataDictionary.h". Use "Midichlorians.h" instead.
// It wraps the inner, platform-independent, event tracking code, and allows client code to stay `.m`.
#error "This C++ header should be `#include`-d or `#import`-ed from an `.mm`, not an `.m` source file."
#endif

#ifndef CURRENT_MIDICHLORIANS_CLIENT_IOS_IMPL_H
#define CURRENT_MIDICHLORIANS_CLIENT_IOS_IMPL_H

#include <string>

#include "../../../Bricks/util/singleton.h"
#include "../../../TypeSystem/Serialization/json.h"
#include "../../../port.h"

#ifdef CURRENT_MIDICHLORIANS_DATA_DICTIONARY_H
#error "The 'MidichloriansDataDictionary.h' file should not be included prior to 'MidichloriansImpl.h'."
#endif  // CURRENT_MIDICHLORIANS_DATA_DICTIONARY_H

#ifndef CURRENT_MAKE_CHECK_MODE

#define COMPILE_MIDICHLORIANS_DATA_DICTIONARY_FOR_IOS_CLIENT
#include "../../MidichloriansDataDictionary.h"

#import <CoreLocation/CoreLocation.h>
#import <Foundation/Foundation.h>

using current::midichlorians::ios::iOSBaseEvent;
using current::midichlorians::ios::iOSFirstLaunchEvent;
using current::midichlorians::ios::iOSAppLaunchEvent;
using current::midichlorians::ios::iOSDeviceInfo;
using current::midichlorians::ios::iOSIdentifyEvent;
using current::midichlorians::ios::iOSFocusEvent;
using current::midichlorians::ios::iOSGenericEvent;
using current::midichlorians::ios::ios_events_t;

// Define the iOS interface, being comfortably within a C++ header file included exclusively from an `.mm` one.
@interface MidichloriansImpl : NSObject

// To be called in application:didFinishLaunchingWithOptions:
// or in application:willFinishLaunchingWithOptions:
+ (void)setup:(NSString*)serverUrl withLaunchOptions:(NSDictionary*)options;

// Emits the event.
+ (void)emit:(const current::midichlorians::ios::iOSBaseEvent&)event;

// Identifies the user.
+ (void)identify:(NSString*)identifier;

@end

#else

// In `CURRENT_MAKE_CHECK_MODE` mode, still compile the data dictionary header.
#include "../../MidichloriansDataDictionary.h"

#endif  // CURRENT_MAKE_CHECK_MODE

#endif  // CURRENT_MIDICHLORIANS_CLIENT_IOS_IMPL_H
