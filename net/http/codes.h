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

#ifndef BRICKS_NET_HTTP_CODES_H
#define BRICKS_NET_HTTP_CODES_H

// HTTP codes: http://www.w3.org/Protocols/rfc2616/rfc2616-sec6.html

#include <map>
#include <string>

#include "../../util/singleton.h"

namespace bricks {
namespace net {

enum class HTTPResponseCodeValue : int {
  InvalidCode = -1,
  Continue = 100,
  SwitchingProtocols = 101,
  OK = 200,
  Created = 201,
  Accepted = 202,
  NonAuthoritativeInformation = 203,
  NoContent = 204,
  ResetContent = 205,
  PartialContent = 206,
  MultipleChoices = 300,
  MovedPermanently = 301,
  Found = 302,
  SeeOther = 303,
  NotModified = 304,
  UseProxy = 305,
  TemporaryRedirect = 307,
  BadRequest = 400,
  Unauthorized = 401,
  PaymentRequired = 402,
  Forbidden = 403,
  NotFound = 404,
  MethodNotAllowed = 405,
  NotAcceptable = 406,
  ProxyAuthenticationRequired = 407,
  RequestTimeout = 408,
  Conflict = 409,
  Gone = 410,
  LengthRequired = 411,
  PreconditionFailed = 412,
  RequestEntityTooLarge = 413,
  RequestURITooLarge = 414,
  UnsupportedMediaType = 415,
  RequestedRangeNotSatisfiable = 416,
  ExpectationFailed = 417,
  InternalServerError = 500,
  NotImplemented = 501,
  BadGateway = 502,
  ServiceUnavailable = 503,
  GatewayTimeout = 504,
  HTTPVersionNotSupported = 505,
};

// Dot notation to allow node.js-friendly syntax of `HTTPResponseCode.OK`, etc.
struct HTTPResponseCodeDotNotation {
  const HTTPResponseCodeValue InvalidCode = HTTPResponseCodeValue::InvalidCode;
  const HTTPResponseCodeValue Continue = HTTPResponseCodeValue::Continue;
  const HTTPResponseCodeValue SwitchingProtocols = HTTPResponseCodeValue::SwitchingProtocols;
  const HTTPResponseCodeValue OK = HTTPResponseCodeValue::OK;
  const HTTPResponseCodeValue Created = HTTPResponseCodeValue::Created;
  const HTTPResponseCodeValue Accepted = HTTPResponseCodeValue::Accepted;
  const HTTPResponseCodeValue NonAuthoritativeInformation = HTTPResponseCodeValue::NonAuthoritativeInformation;
  const HTTPResponseCodeValue NoContent = HTTPResponseCodeValue::NoContent;
  const HTTPResponseCodeValue ResetContent = HTTPResponseCodeValue::ResetContent;
  const HTTPResponseCodeValue PartialContent = HTTPResponseCodeValue::PartialContent;
  const HTTPResponseCodeValue MultipleChoices = HTTPResponseCodeValue::MultipleChoices;
  const HTTPResponseCodeValue MovedPermanently = HTTPResponseCodeValue::MovedPermanently;
  const HTTPResponseCodeValue Found = HTTPResponseCodeValue::Found;
  const HTTPResponseCodeValue SeeOther = HTTPResponseCodeValue::SeeOther;
  const HTTPResponseCodeValue NotModified = HTTPResponseCodeValue::NotModified;
  const HTTPResponseCodeValue UseProxy = HTTPResponseCodeValue::UseProxy;
  const HTTPResponseCodeValue TemporaryRedirect = HTTPResponseCodeValue::TemporaryRedirect;
  const HTTPResponseCodeValue BadRequest = HTTPResponseCodeValue::BadRequest;
  const HTTPResponseCodeValue Unauthorized = HTTPResponseCodeValue::Unauthorized;
  const HTTPResponseCodeValue PaymentRequired = HTTPResponseCodeValue::PaymentRequired;
  const HTTPResponseCodeValue Forbidden = HTTPResponseCodeValue::Forbidden;
  const HTTPResponseCodeValue NotFound = HTTPResponseCodeValue::NotFound;
  const HTTPResponseCodeValue MethodNotAllowed = HTTPResponseCodeValue::MethodNotAllowed;
  const HTTPResponseCodeValue NotAcceptable = HTTPResponseCodeValue::NotAcceptable;
  const HTTPResponseCodeValue ProxyAuthenticationRequired = HTTPResponseCodeValue::ProxyAuthenticationRequired;
  const HTTPResponseCodeValue RequestTimeout = HTTPResponseCodeValue::RequestTimeout;
  const HTTPResponseCodeValue Conflict = HTTPResponseCodeValue::Conflict;
  const HTTPResponseCodeValue Gone = HTTPResponseCodeValue::Gone;
  const HTTPResponseCodeValue LengthRequired = HTTPResponseCodeValue::LengthRequired;
  const HTTPResponseCodeValue PreconditionFailed = HTTPResponseCodeValue::PreconditionFailed;
  const HTTPResponseCodeValue RequestEntityTooLarge = HTTPResponseCodeValue::RequestEntityTooLarge;
  const HTTPResponseCodeValue RequestURITooLarge = HTTPResponseCodeValue::RequestURITooLarge;
  const HTTPResponseCodeValue UnsupportedMediaType = HTTPResponseCodeValue::UnsupportedMediaType;
  const HTTPResponseCodeValue RequestedRangeNotSatisfiable =
      HTTPResponseCodeValue::RequestedRangeNotSatisfiable;
  const HTTPResponseCodeValue ExpectationFailed = HTTPResponseCodeValue::ExpectationFailed;
  const HTTPResponseCodeValue InternalServerError = HTTPResponseCodeValue::InternalServerError;
  const HTTPResponseCodeValue NotImplemented = HTTPResponseCodeValue::NotImplemented;
  const HTTPResponseCodeValue BadGateway = HTTPResponseCodeValue::BadGateway;
  const HTTPResponseCodeValue ServiceUnavailable = HTTPResponseCodeValue::ServiceUnavailable;
  const HTTPResponseCodeValue GatewayTimeout = HTTPResponseCodeValue::GatewayTimeout;
  const HTTPResponseCodeValue HTTPVersionNotSupported = HTTPResponseCodeValue::HTTPVersionNotSupported;
  inline HTTPResponseCodeValue operator()(int code) { return static_cast<HTTPResponseCodeValue>(code); }
};

#define HTTPResponseCode (bricks::Singleton<bricks::net::HTTPResponseCodeDotNotation>())

inline std::string HTTPResponseCodeAsString(HTTPResponseCodeValue code) {
  static const std::map<int, std::string> codes = {
      {100, "Continue"},
      {101, "Switching Protocols"},
      {200, "OK"},
      {201, "Created"},
      {202, "Accepted"},
      {203, "Non-Authoritative Information"},
      {204, "No Content"},
      {205, "Reset Content"},
      {206, "Partial Content"},
      {300, "Multiple Choices"},
      {301, "Moved Permanently"},
      {302, "Found"},
      {303, "See Other"},
      {304, "Not Modified"},
      {305, "Use Proxy"},
      {307, "Temporary Redirect"},
      {400, "Bad Request"},
      {401, "Unauthorized"},
      {402, "Payment Required"},
      {403, "Forbidden"},
      {404, "Not Found"},
      {405, "Method Not Allowed"},
      {406, "Not Acceptable"},
      {407, "Proxy Authentication Required"},
      {408, "Request Time-out"},
      {409, "Conflict"},
      {410, "Gone"},
      {411, "Length Required"},
      {412, "Precondition Failed"},
      {413, "Request Entity Too Large"},
      {414, "Request-URI Too Large"},
      {415, "Unsupported Media Type"},
      {416, "Requested range not satisfiable"},
      {417, "Expectation Failed"},
      {500, "Internal Server Error"},
      {501, "Not Implemented"},
      {502, "Bad Gateway"},
      {503, "Service Unavailable"},
      {504, "Gateway Time-out"},
      {505, "HTTP Version not supported"},
      {-1, "<UNINITIALIZED>"}  // `-1` is used internally by Bricks as `uninitialized`.
  };
  const auto cit = codes.find(static_cast<int>(code));
  if (cit != codes.end()) {
    return cit->second;
  } else {
    return "Unknown Code";
  }
}

}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_HTTP_CODES_H
