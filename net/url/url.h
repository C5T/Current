/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev, <dmitry.korolev@gmail.com>.

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

#ifndef BRICKS_NET_URL_URL_H
#define BRICKS_NET_URL_URL_H

#include <algorithm>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>

#include "../../exception.h"

namespace bricks {
namespace net {
namespace url {

struct EmptyURLException : Exception {};
struct EmptyURLHostException : Exception {};

// URL manages the mapping between the string and parsed representations of the URL. It manages:
//
// * host     (string)
// * path     (string, defaults to "/", never empty).
// * protocol (defaults to "http", empty only if set explicitly in constructor).
// * port (defaults to the default port for supported protocols).
//
// When handling redirects, the previous URL can be provided to properly handle host/port/protocol.

namespace impl {

namespace {
const char* const kDefaultProtocol = "http";
}

struct URLWithoutParameters {
  std::string host = "";
  std::string path = "/";
  std::string protocol = kDefaultProtocol;
  int port = 0;

  URLWithoutParameters() = default;

  // Extra parameters for previous host and port are provided in the constructor to handle redirects.
  URLWithoutParameters(const std::string& url,
                       const std::string& previous_protocol = kDefaultProtocol,
                       const std::string& previous_host = "",
                       const int previous_port = 0) {
    if (url.empty()) {
      throw EmptyURLException();
    }
    protocol = "";
    size_t offset_past_protocol = 0;
    const size_t i = url.find("://");
    if (i != std::string::npos) {
      protocol = url.substr(0, i);
      offset_past_protocol = i + 3;
    }

    const size_t colon = url.find(':', offset_past_protocol);
    const size_t slash = url.find('/', offset_past_protocol);
    host = url.substr(offset_past_protocol, std::min(colon, slash) - offset_past_protocol);
    if (host.empty()) {
      host = previous_host;
      if (host.empty()) {
        throw EmptyURLHostException();
      }
    }

    if (colon < slash) {
      port = atoi(url.c_str() + colon + 1);
    } else {
      port = previous_port;
    }

    if (slash != std::string::npos) {
      path = url.substr(slash);
    } else {
      path = "";
    }
    if (path.empty()) {
      path = "/";
    }

    if (protocol.empty()) {
      if (!previous_protocol.empty()) {
        protocol = previous_protocol;
      } else {
        protocol = DefaultProtocolForPort(port);
      }
    }

    if (port == 0) {
      port = DefaultPortForProtocol(protocol);
    }
  }

  URLWithoutParameters(const std::string& url, const URLWithoutParameters& previous)
      : URLWithoutParameters(url, previous.protocol, previous.host, previous.port) {}

  std::string ComposeURL() const {
    std::ostringstream os;
    if (!protocol.empty()) {
      os << protocol << "://";
    }
    os << host;
    if (port != DefaultPortForProtocol(protocol)) {
      os << ':' << port;
    }
    os << path;
    return os.str();
  }

  static int DefaultPortForProtocol(const std::string& protocol) {
    // We don't really support HTTPS/SSL or any other protocols yet -- D.K. :-)
    return protocol == "http" ? 80 : 0;
  }

  static std::string DefaultProtocolForPort(int port) { return port == 80 ? "http" : ""; }
};

struct URLParameters {
  URLParameters() = default;
  URLParameters(const std::string& url_string) : url_without_parameters_(url_string) {}
  std::string url_without_parameters_;
};

struct URL : URLParameters, URLWithoutParameters {
  URL() = default;

  // Extra parameters for previous host and port are provided in the constructor to handle redirects.
  URL(const std::string& url,
      const std::string& previous_protocol = kDefaultProtocol,
      const std::string& previous_host = "",
      const int previous_port = 0)
      : URLParameters(url),
        URLWithoutParameters(
            URLParameters::url_without_parameters_, previous_protocol, previous_host, previous_port) {}

  URL(const std::string& url, const URLWithoutParameters& previous)
      : URLParameters(url), URLWithoutParameters(URLParameters::url_without_parameters_, previous) {}
};

}  // namespace impl

typedef impl::URL URL;

}  // namespace url
}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_URL_URL_H
