/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_NET_HTTP_DEFAULT_MESSAGES_H
#define BRICKS_NET_HTTP_DEFAULT_MESSAGES_H

#include "../../port.h"

#include <string>

namespace current {
namespace net {

// Looks plausible to keep error messages capitalized, with a newline at and end, and wrapped into an <h1>.
// Even though Bricks is mostly for backends, if we make them appear as JSON-s,
// along the lines of `{"error":404}`, our JSON-s are based on schemas, so that won't add much value.
// Thus, just keep them simple, unambiguous, curl- and browser-friendy for now -- D.K.
inline std::string DefaultFourOhFourMessage() { return "<h1>NOT FOUND</h1>\n"; }
inline std::string DefaultInternalServerErrorMessage() { return "<h1>INTERNAL SERVER ERROR</h1>\n"; }
inline std::string DefaultMethodNotAllowedMessage() { return "<h1>METHOD NOT ALLOWED</h1>\n"; }

}  // namespace net
}  // namespace current

#endif  // BRICKS_NET_HTTP_DEFAULT_MESSAGES_H
