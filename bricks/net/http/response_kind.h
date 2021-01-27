/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2021 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_NET_HTTP_RESPONSE_KIND_H
#define BRICKS_NET_HTTP_RESPONSE_KIND_H

#include "../exceptions.h"

namespace current {
namespace net {

// HTTPResponseKind: `Regular`, `Chunked`, or `Upgraded`.
class HTTPResponseKind final {
 private:
  enum class Kind { Regular, Chunked, Upgraded };
  Kind kind_ = Kind::Regular;

 public:
  bool IsChunked() const { return kind_ == Kind::Chunked; }
  bool IsUpgraded() const { return kind_ == Kind::Upgraded; }
  void MarkAsChunked() {
    if (kind_ == Kind::Regular) {
      kind_ = Kind::Chunked;
    } else if (kind_ == Kind::Chunked) {
      return;
    } else {
      CURRENT_THROW(ChunkedOrUpgradedHTTPResponseRequestedMismatch());
    }
  }
  void MarkAsUpgraded() {
    if (kind_ == Kind::Regular) {
      kind_ = Kind::Upgraded;
    } else if (kind_ == Kind::Upgraded) {
      return;
    } else {
      CURRENT_THROW(ChunkedOrUpgradedHTTPResponseRequestedMismatch());
    }
  }
};

}  // namespace net
}  // namespace current

#endif  // BRICKS_NET_HTTP_RESPONSE_KIND_H
