/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_KARL_EXCEPTIONS_H
#define CURRENT_KARL_EXCEPTIONS_H

#include "../blocks/graceful_shutdown/exceptions.h"

namespace current {
namespace karl {

struct KarlException : Exception {
  using Exception::Exception;
};

struct ClaireRegistrationException : KarlException {
  ClaireRegistrationException(const std::string& service, const std::string& route, const std::string& message = "")
      : KarlException(service + " @ " + route + (message.empty() ? "" : " # " + message)) {}
};

struct NginxRequestedButNotAvailableException : KarlException {
  using KarlException::KarlException;
};

struct NginxParametersInvalidPortException : KarlException {
  using KarlException::KarlException;
};

}  // namespace karl
}  // namespace current

#endif  // CURRENT_KARL_EXCEPTIONS_H
