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

#if defined(BRICKS_APPLE)

#if ! __has_feature(objc_arc)
#error "This file must be compiled with ARC. Either turn on ARC for the project or use -fobjc-arc flag."
#endif

#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSData.h>
#import <Foundation/NSStream.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLResponse.h>
#import <Foundation/NSURLConnection.h>
#import <Foundation/NSOperation.h>
#import <Foundation/NSError.h>
#import <Foundation/NSURLError.h>
#import <Foundation/NSFileManager.h>

#define TIMEOUT_IN_SECONDS 30.0

bool bricks::net::api::HTTPClientApple::Go() {
  @autoreleasepool {

    NSMutableURLRequest * request = [NSMutableURLRequest requestWithURL:
        [NSURL URLWithString:[NSString stringWithUTF8String:url_requested.c_str()]]
        cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:TIMEOUT_IN_SECONDS];

    if (!content_type.empty()) {
      [request setValue:[NSString stringWithUTF8String:content_type.c_str()] forHTTPHeaderField:@"Content-Type"];
    }

    if (!user_agent.empty()) {
      [request setValue:[NSString stringWithUTF8String:user_agent.c_str()] forHTTPHeaderField:@"User-Agent"];
    }

    if (!method.empty()) {
      request.HTTPMethod = [NSString stringWithUTF8String:method.c_str()];
    } else {
      http_response_code = -1;
      NSLog(@"HTTP method is empty.");
      return false;
    }

    if (method == "POST" || method == "PUT") {
      if (!post_file.empty()) {
        NSError * err = nil;
        NSString * path = [NSString stringWithUTF8String:post_file.c_str()];
        const unsigned long long file_size =
          [[NSFileManager defaultManager] attributesOfItemAtPath:path error:&err].fileSize;
        if (err) {
          http_response_code = -1;
          NSLog(@"Error %d %@", static_cast<int>(err.code), err.localizedDescription);
          return false;
        }
        request.HTTPBodyStream = [NSInputStream inputStreamWithFileAtPath:path];
        [request setValue:[NSString stringWithFormat:@"%llu", file_size] forHTTPHeaderField:@"Content-Length"];
      } else {
        request.HTTPBody = [NSData dataWithBytes:post_body.data() length:post_body.size()];
      }
    }

    *async_request_completed.MutableScopedAccessor() = false;
    [NSURLConnection sendAsynchronousRequest:request queue:[[NSOperationQueue alloc] init]
        completionHandler:^(NSURLResponse *response, NSData *data, NSError *error) {
          // Workaround to handle HTTP 401 response without errors.
          if (error.code == NSURLErrorUserCancelledAuthentication) {
            http_response_code = 401;
            request_succeeded = true;
          } else {
            if (response) {
              NSHTTPURLResponse *http_response = (NSHTTPURLResponse *)response;
              http_response_code = http_response.statusCode;
              url_received = [http_response.URL.absoluteString UTF8String];
              request_succeeded = true;
            } else {
              http_response_code = -1;
              NSLog(@"ERROR while connecting to %s: %@", url_requested.c_str(), error.localizedDescription);
              request_succeeded = false;
            }
          }

          if (data) {
            if (received_file.empty()) {
              server_response.assign(reinterpret_cast<char const *>(data.bytes), data.length);
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
    }];

    async_request_completed.Wait([](bool done) { return done; });

    return request_succeeded;

  } // @autoreleasepool
}

#endif  // defined(BRICKS_APPLE)
