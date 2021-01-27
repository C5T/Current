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

#include "../../../bricks/strings/strings.h"

// HTTP constants to parse the header and extract method, URL, headers and body.

namespace current {
namespace net {
namespace constants {

constexpr char kCRLF[] = "\r\n";
constexpr size_t kCRLFLength = strings::CompileTimeStringLength(kCRLF);

constexpr char kDefaultContentType[] = "text/plain";
constexpr char kDefaultJSONContentType[] = "application/json; charset=utf-8";
// TODO(dkorolev): Make use of this constant everywhere.
constexpr char kDefaultHTMLContentType[] = "text/html; charset=utf-8";
constexpr char kDefaultSVGContentType[] = "image/svg+xml; charset=utf-8";
constexpr char kDefaultPNGContentType[] = "image/png";
constexpr char kDefaultJSONStreamContentType[] = "application/stream+json; charset=utf-8";

constexpr char kHeaderKeyValueSeparator = ':';

constexpr char kContentLengthHeaderKey[] = "Content-Length";
constexpr char kTransferEncodingHeaderKey[] = "Transfer-Encoding";
constexpr char kTransferEncodingChunkedValue[] = "chunked";
constexpr char kHTTPMethodOverrideHeaderKey[] = "X-HTTP-Method-Override";

constexpr char kUpgradeHeaderKey[] = "Upgrade";
constexpr char kConnectionHeaderKey[] = "Connection";
constexpr char kConnectionUpgradeChunkedValue[] = "upgrade";  // For `Connection: upgrade` to work as `Upgrade: ...`.

// By default:
// * HTTP responses that use `struct Response` will have the CORS header set.
//   It can be unset with `.DisableCORS()`. There is also `EnableCORS()` to add it back.
// * HTTP responses that use `Request::operator()` will not have the CORS header set.
//   Use `Headers.{Set,Remove}CORSHeader()` to manually insert or remove it.
constexpr char kCORSHeaderName[] = "Access-Control-Allow-Origin";
constexpr char kCORSHeaderValue[] = "*";

#ifndef CURRENT_MAX_HTTP_PAYLOAD
constexpr size_t kMaxHTTPPayloadSizeInBytes = 1024 * 1024 * 16;
#else
constexpr size_t kMaxHTTPPayloadSizeInBytes = CURRENT_MAX_HTTP_PAYLOAD;
#endif  // CURRENT_MAX_HTTP_PAYLOAD

}  // namespace constants
}  // namespace net
}  // namespace current

#endif  // BRICKS_NET_HTTP_CONSTANTS_H
