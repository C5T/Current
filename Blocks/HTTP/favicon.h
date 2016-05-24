/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev, <dmitry.korolev@gmail.com>.

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

#ifndef BLOCKS_HTTP_FAVICON_H
#define BLOCKS_HTTP_FAVICON_H

#include "../port.h"

#include <string>

#include "response.h"

#include "../../Bricks/util/base64.h"

namespace current {
namespace http {

// clang-format off
const char* const kCurrentFaviconPngBase64 = "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";
// clang-format on

inline const char* CurrentFaviconPngBase64() { return kCurrentFaviconPngBase64; }

inline std::string CurrentFaviconPng() {
  static const std::string png = Base64Decode(CurrentFaviconPngBase64());
  return png;
}

inline Response CurrentFaviconResponse() {
  return Response(CurrentFaviconPng(), HTTPResponseCode.OK, "image/png");
}

inline std::function<void(Request)> CurrentFaviconHandler() {
  return [](Request r) { r(CurrentFaviconResponse()); };
}

}  // namespace http
}  // namespace current

#endif  // BLOCKS_HTTP_FAVICON_H
