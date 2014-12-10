/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Alexander Zolotarev <me@alex.bio> from Minsk, Belarus

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

#if ! __has_feature(objc_arc)
#error This file must be compiled with ARC. Either turn on ARC for the project or use -fobjc-arc flag
#endif

#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSData.h>
#import <Foundation/NSStream.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLResponse.h>
#import <Foundation/NSURLConnection.h>
#import <Foundation/NSError.h>
#import <Foundation/NSFileManager.h>

#include "http_client.h"


#define TIMEOUT_IN_SECONDS 30.0

namespace aloha {

bool HTTPClientPlatformWrapper::RunHTTPRequest() {
  @autoreleasepool {

    NSMutableURLRequest * request = [NSMutableURLRequest requestWithURL:
        [NSURL URLWithString:[NSString stringWithUTF8String:url_requested_.c_str()]]
        cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:TIMEOUT_IN_SECONDS];

    if (!content_type_.empty())
      [request setValue:[NSString stringWithUTF8String:content_type_.c_str()] forHTTPHeaderField:@"Content-Type"];
    if (!user_agent_.empty())
      [request setValue:[NSString stringWithUTF8String:user_agent_.c_str()] forHTTPHeaderField:@"User-Agent"];

    if (!post_body_.empty()) {
      request.HTTPBody = [NSData dataWithBytes:post_body_.data() length:post_body_.size()];
      request.HTTPMethod = @"POST";
    } else if (!post_file_.empty()) {
      NSError * err = nil;
      NSString * path = [NSString stringWithUTF8String:post_file_.c_str()];
      const unsigned long long file_size = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:&err].fileSize;
      if (err) {
        error_code_ = err.code;
        NSLog(@"Error %d %@", error_code_, err.localizedDescription);
        return false;
      }
      request.HTTPBodyStream = [NSInputStream inputStreamWithFileAtPath:path];
      request.HTTPMethod = @"POST";
      [request setValue:[NSString stringWithFormat:@"%llu", file_size] forHTTPHeaderField:@"Content-Length"];
    }

    NSHTTPURLResponse * response = nil;
    NSError * err = nil;
    NSData * url_data = [NSURLConnection sendSynchronousRequest:request returningResponse:&response error:&err];

    if (response) {
      error_code_ = response.statusCode;
      url_received_ = [response.URL.absoluteString UTF8String];
    }
    else {
      error_code_ = err.code;
      NSLog(@"ERROR while connecting to %s: %@", url_requested_.c_str(), err.localizedDescription);
    }

    if (url_data) {
      if (received_file_.empty())
        server_response_.assign(reinterpret_cast<char const *>(url_data.bytes), url_data.length);
      else
        [url_data writeToFile:[NSString stringWithUTF8String:received_file_.c_str()] atomically:YES];
    }
    return true;  // TODO(dkorolev) + TODO(deathbaba): Figure out something smarter than return (200 == error_code_);

  } // @autoreleasepool
}

} // namespace aloha
