/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_TEST_COMPILATION

#import <Foundation/Foundation.h>

// In-house events tracking. Objective-C wrapper class. All methods are static class methods.

@interface Midichlorians : NSObject

// Initialization, to be called from application:{didFinishLaunchingWithOptions|willFinishLaunchingWithOptions}.
+ (void)setup:(NSString *)serverUrl withLaunchOptions:(NSDictionary *)options;

// Client identification call, structured.
+ (void)identify:(NSString *)identifier;

// iOS focus events tracking, structured.
// Can be called from anywhere after `setup` has been called.
// Usually called from `AppDelegate.m`-s event handlers.
+ (void)focusEvent:(BOOL)hasFocus source:(NSString *)source;

// User events tracking, unstructured.
// Can be called from anywhere after `setup` has been called.
+ (void)trackEvent:(NSString *)event source:(NSString *)eventSource properties:(NSDictionary *)eventProperties;

@end

#endif  // CURRENT_TEST_COMPILATION
