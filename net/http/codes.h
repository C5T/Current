#ifndef BRICKS_NET_HTTP_CODES_H
#define BRICKS_NET_HTTP_CODES_H

// HTTP codes: http://www.w3.org/Protocols/rfc2616/rfc2616-sec6.html

#include <map>
#include <string>

namespace bricks {
namespace net {

enum class HTTPResponseCode : int {
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

class HTTPResponseCodeAsStringGenerator {
 public:
  static inline std::string CodeAsString(HTTPResponseCode code) {
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
    };
    const auto cit = codes.find(static_cast<int>(code));
    if (cit != codes.end()) {
      return cit->second;
    } else {
      return "Unknown Code";
    }
  }
};

}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_HTTP_CODES_H
