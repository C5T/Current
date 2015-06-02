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
// If you get this error build your Objective-C project, #include neither this file nor "MidichloriansImpl.h".
// Use "Midichlorians.h" instead.
// It wraps the inner, plarform-independent, event tracking code, and allows client code to stay `.m`.
#error  "This file is a C++ header, and it should be `#include`-d or `#import`-ed from an `.mm`, not an `.m` source file."
#endif

#ifndef CURRENT_MIDICHLORIANS_IMPL_H
#define CURRENT_MIDICHLORIANS_IMPL_H

#include <string>

#include "../../../../Bricks/cerealize/cerealize.h"

#import <Foundation/Foundation.h>
#import <CoreLocation/CoreLocation.h>

#ifdef CURRENT_MIDICHLORIANS_DATA_DICTIONARY_H
#error "The 'MidichloriansDataDictionary.h' file should not be included prior to 'Midichlodians[Impl].h'."
#endif  // CURRENT_MIDICHLORIANS_DATA_DICTIONARY_H

#define COMPILE_MIDICHLORIANS_DATA_DICTIONARY_FOR_IOS_CLIENT
#include "../MidichloriansDataDictionary.h"

// Define the iOS interface, being comfortably within a C++ header file included exclusively from an `.mm` one.
@interface MidichloriansImpl : NSObject

// To be called in application:didFinishLaunchingWithOptions:
// or in application:willFinishLaunchingWithOptions:
+ (void)setup:(NSString*)serverUrl withLaunchOptions:(NSDictionary*)options;

// Emits the event.
+ (void)emit:(const MidichloriansEvent&)event;

// Identifies the user.
+ (void)identify:(NSString*)identifier;

@end

#endif  // CURRENT_MIDICHLORIANS_IMPL_H
