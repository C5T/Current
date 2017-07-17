/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Alexander Zolotarev <me@alex.bio> from Minsk, Belarus
          (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

// TODO(mzhurovich): Add support of chunked response and redirection control.
// TODO(mzhurovich): Throw proper exceptions to pass the temporary disabled tests.

#include "../../../port.h"

#if defined(CURRENT_APPLE)

#if ! __has_feature(objc_arc)
#error "This file must be compiled with ARC. Either turn on ARC for the project or use -fobjc-arc flag."
#endif

#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSData.h>
#import <Foundation/NSStream.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLResponse.h>
#import <Foundation/NSURLSession.h>
#import <Foundation/NSHTTPCookie.h>
#import <Foundation/NSOperation.h>
#import <Foundation/NSError.h>
#import <Foundation/NSURLError.h>
#import <Foundation/NSFileManager.h>

#define TIMEOUT_IN_SECONDS 30.0

bool current::http::HTTPClientApple::Go() {
  @autoreleasepool {

    NSMutableURLRequest * request = [NSMutableURLRequest requestWithURL:
        [NSURL URLWithString:[NSString stringWithUTF8String:request_url.c_str()]]
        cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:TIMEOUT_IN_SECONDS];

    if (!request_body_content_type.empty()) {
      [request setValue:[NSString stringWithUTF8String:request_body_content_type.c_str()]
          forHTTPHeaderField:@"Content-Type"];
    }

    if (!request_user_agent.empty()) {
      [request setValue:[NSString stringWithUTF8String:request_user_agent.c_str()]
          forHTTPHeaderField:@"User-Agent"];
    }

    for (const auto& h : request_headers) {
      [request setValue:[NSString stringWithUTF8String:h.value.c_str()]
          forHTTPHeaderField:[NSString stringWithUTF8String:h.header.c_str()]];
    }
    if (!request_headers.cookies.empty()) {
      [request setValue:[NSString stringWithUTF8String:request_headers.CookiesAsString().c_str()]
          forHTTPHeaderField:@"Cookie"];
    }

    if (!request_method.empty()) {
      request.HTTPMethod = [NSString stringWithUTF8String:request_method.c_str()];
    } else {
      response_code = -1;
      CURRENT_NSLOG(@"HTTP request_method is empty.");
      return false;
    }

    if (request_method == "POST" || request_method == "PUT") {
      if (!post_file.empty()) {
        NSError * err = nil;
        NSString * path = [NSString stringWithUTF8String:post_file.c_str()];
        const unsigned long long file_size =
          [[NSFileManager defaultManager] attributesOfItemAtPath:path error:&err].fileSize;
        if (err) {
          response_code = -1;
          CURRENT_NSLOG(@"Error %d %@", static_cast<int>(err.code), err.localizedDescription);
          return false;
        }
        request.HTTPBodyStream = [NSInputStream inputStreamWithFileAtPath:path];
        [request setValue:[NSString stringWithFormat:@"%llu", file_size] forHTTPHeaderField:@"Content-Length"];
      } else {
        request.HTTPBody = [NSData dataWithBytes:request_body.data() length:request_body.size()];
      }
    }

    *async_request_completed.MutableScopedAccessor() = false;
    NSURLSessionConfiguration *configuration = [NSURLSessionConfiguration defaultSessionConfiguration];
    configuration.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
    configuration.HTTPCookieAcceptPolicy = NSHTTPCookieAcceptPolicyNever;
    configuration.URLCache = nil;
    configuration.URLCredentialStorage = nil;
    configuration.HTTPCookieStorage = nil;
    NSURLSession *session = [NSURLSession sessionWithConfiguration:configuration];
    [[session dataTaskWithRequest:request
        completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
          // Workaround to handle HTTP 401 response without errors.
          if (error.code == NSURLErrorUserCancelledAuthentication) {
            response_code = 401;
            request_succeeded = true;
          } else {
            if (response) {
              NSHTTPURLResponse *http_response = (NSHTTPURLResponse *)response;
              response_code = http_response.statusCode;
              response_url = [http_response.URL.absoluteString UTF8String];
              NSDictionary *headers = http_response.allHeaderFields;
              for (NSString *key in headers) {
                if ([key caseInsensitiveCompare:@"Set-Cookie"] != NSOrderedSame) {
                  response_headers.Set([key UTF8String], [[headers objectForKey:key] UTF8String]);
                }
              }
              NSArray *cookies = [NSHTTPCookie
                  cookiesWithResponseHeaderFields:headers forURL:[NSURL URLWithString:@""]];
              for (NSHTTPCookie *cookie in cookies) {
                response_headers.SetCookie([[cookie name] UTF8String], [[cookie value] UTF8String]);
              }
              // TODO(mzhurovich): Support optional cookie attributes.
              request_succeeded = true;
            } else {
              response_code = -1;
              CURRENT_NSLOG(@"ERROR while connecting to %s: %@", request_url.c_str(), error.localizedDescription);
              request_succeeded = false;
            }
          }

          if (data) {
            if (received_file.empty()) {
              response_body.assign(reinterpret_cast<char const *>(data.bytes), data.length);
              request_succeeded = true;
            } else {
              if ([data writeToFile:[NSString stringWithUTF8String:received_file.c_str()] atomically:YES]) {
                request_succeeded = true;
              } else {
                request_succeeded = false;
              }
            }
          }
          *async_request_completed.MutableScopedAccessor() = true;
    }] resume];

    async_request_completed.Wait([](bool done) { return done; });

    return request_succeeded;

  } // @autoreleasepool
}

#endif  // defined(CURRENT_APPLE)
