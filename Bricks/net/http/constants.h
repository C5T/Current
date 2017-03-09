/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2017 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_NET_HTTP_CONSTANTS_H
#define BRICKS_NET_HTTP_CONSTANTS_H

#include "../../../Bricks/strings/strings.h"

// HTTP constants to parse the header and extract method, URL, headers and body.

namespace current {
namespace net {
namespace constants {

constexpr static const char kCRLF[] = "\r\n";
constexpr const size_t kCRLFLength = strings::CompileTimeStringLength(kCRLF);

constexpr const char* kDefaultContentType = "text/plain";
constexpr const char* kDefaultJSONContentType = "application/json; charset=utf-8";
// TODO(dkorolev): Make use of this constant everywhere.
constexpr const char* kDefaultHTMLContentType = "text/html; charset=utf-8";
constexpr const char* kDefaultSVGContentType = "image/svg+xml; charset=utf-8";
constexpr const char* kDefaultPNGContentType = "image/png";

constexpr const char kHeaderKeyValueSeparator[] = ": ";
constexpr const size_t kHeaderKeyValueSeparatorLength = strings::CompileTimeStringLength(kHeaderKeyValueSeparator);

constexpr const char* const kContentLengthHeaderKey = "Content-Length";
constexpr const char* const kTransferEncodingHeaderKey = "Transfer-Encoding";
constexpr const char* const kTransferEncodingChunkedValue = "chunked";
constexpr const char* const kHTTPMethodOverrideHeaderKey = "X-HTTP-Method-Override";

constexpr const char* const kHTTPAccessControlAllowOriginHeaderName = "Access-Control-Allow-Origin";
constexpr const char* const kHTTPAccessControlAllowOriginHeaderValue = "*";

#ifndef CURRENT_MAX_HTTP_PAYLOAD
constexpr const size_t kMaxHTTPPayloadSizeInBytes = 1024 * 1024 * 16;
#else
constexpr const size_t kMaxHTTPPayloadSizeInBytes = CURRENT_MAX_HTTP_PAYLOAD;
#endif  // CURRENT_MAX_HTTP_PAYLOAD

}  // namespace constants
}  // namespace net
}  // namespace current

#endif  // BRICKS_NET_HTTP_CONSTANTS_H
