#ifndef BRICKS_NET_API_URL_H
#define BRICKS_NET_API_URL_H

#include <algorithm>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>

using std::max;
using std::min;
using std::ostringstream;
using std::string;
using std::vector;

namespace bricks {
namespace net {
namespace api {

// Initialize or inherit from URLParser to be able to call `ParseURL(url)` and use:
//
// * host (defaults to "localhost", never empty).
// * path (defaults to "/", never empty).
// * protocol (defaults to "http", never empty).
// * port (defaults to the default port for supported protocols).
//
// Alternatively, previous URL can be provided to properly handle redirect URLs with omitted fields.

namespace {

const char* const kDefaultProtocol = "http";
const char* const kDefaultHost = "localhost";
}

struct URLParser {
  string host = kDefaultHost;
  string path = "/";
  string protocol = kDefaultProtocol;
  int port = 0;

  URLParser() = default;

  // Extra parameters for previous host and port are provided in the constructor to handle redirects.
  URLParser(const string& url,
            const string& previous_protocol = kDefaultProtocol,
            const string& previous_host = kDefaultHost,
            const int previous_port = 0) {
    protocol = "";  //"http";
    size_t offset_past_protocol = 0;
    const size_t i = url.find("://");
    if (i != string::npos) {
      protocol = url.substr(0, i);
      offset_past_protocol = i + 3;
    }

    const size_t colon = url.find(':', offset_past_protocol);
    const size_t slash = url.find('/', offset_past_protocol);
    host = url.substr(offset_past_protocol, min(colon, slash) - offset_past_protocol);
    if (host.empty()) {
      host = previous_host;
    }

    if (colon < slash) {
      port = atoi(url.c_str() + colon + 1);
    } else {
      port = previous_port;
    }

    if (slash != string::npos) {
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
        if (protocol.empty()) {
          protocol = kDefaultProtocol;
        }
      }
    }

    if (port == 0) {
      port = DefaultPortForProtocol(protocol);
    }
  }

  URLParser(const string& url, const URLParser& previous)
      : URLParser(url, previous.protocol, previous.host, previous.port) {
  }

  string ComposeURL() const {
    ostringstream os;
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

  static int DefaultPortForProtocol(const string& protocol) {
    // We don't really support HTTPS/SSL or any other protocols yet -- D.K. :-)
    return protocol == "http" ? 80 : 0;
  }

  static string DefaultProtocolForPort(int port) {
    return port == 80 ? "http" : "";
  }
};

}  // namespace api
}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_API_URL_H
