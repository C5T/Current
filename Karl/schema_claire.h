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

#ifndef KARL_SCHEMA_CLAIRE_H
#define KARL_SCHEMA_CLAIRE_H

#include "../port.h"

// The `current_build.h` file from this local `Current/Karl` dir makes no sense for external users of Karl.
// Nonetheless, top-level `make test` and `make check` should pass out of the box.
#ifdef CURRENT_MAKE_CHECK_MODE
#include "current_build.h.mock"
#endif

#include "../TypeSystem/struct.h"
#include "../TypeSystem/optional.h"
#include "../TypeSystem/variant.h"

#include "../Blocks/URL/url.h"

#include "../bricks/time/chrono.h"

#include "../bricks/net/http/impl/server.h"  // current::net::constants

namespace current {
namespace karl {

CURRENT_STRUCT(ClaireServiceKey) {
  CURRENT_FIELD(ip, std::string);
  CURRENT_FIELD_DESCRIPTION(ip, "The IP address of the server on which the service is running.");

  CURRENT_FIELD(port, uint16_t);
  CURRENT_FIELD_DESCRIPTION(port, "The local port on which the service is running.");

  // TODO(dkorolev) + TODO(mzhurovich): Should or should not end with '/'?
  CURRENT_FIELD(prefix, std::string, "");
  CURRENT_FIELD_DESCRIPTION(prefix,
                            "The URL prefix for the status page of the service, "
                            "in cases when multiple services share the same port.");

  CURRENT_DEFAULT_CONSTRUCTOR(ClaireServiceKey) {}
  CURRENT_CONSTRUCTOR(ClaireServiceKey)(const ClaireServiceKey& rhs) : ip(rhs.ip), port(rhs.port), prefix(rhs.prefix) {}
  CURRENT_CONSTRUCTOR(ClaireServiceKey)(const std::string& url) {
    URL decomposed(url);
    ip = current::net::ResolveIPFromHostname(decomposed.host);
    port = decomposed.port;
    prefix = decomposed.path;
  }

  std::tuple<std::string, uint16_t, std::string> AsTuple() const { return std::tie(ip, port, prefix); }
  bool operator==(const ClaireServiceKey& rhs) const { return AsTuple() == rhs.AsTuple(); }
  bool operator!=(const ClaireServiceKey& rhs) const { return !operator==(rhs); }
  bool operator<(const ClaireServiceKey& rhs) const { return AsTuple() < rhs.AsTuple(); }

  std::string StatusPageURL() const { return "http://" + ip + ':' + current::ToString(port) + prefix + ".current"; }
};

// clang-format off
CURRENT_ENUM(KeepaliveAttemptStatus, uint8_t) {
  Unknown = 0u,
  Success = 1u,
  CouldNotConnect = 2u,
  ErrorCodeReturned = 3u
};
// clang-format on

// The generic status.
// Persisted by Karl, except for the `build` part, which is only persisted on the first call, or if changed.
CURRENT_STRUCT(ClaireStatus) {
  CURRENT_FIELD(service, std::string);
  CURRENT_FIELD_DESCRIPTION(service, "The name of the service, as christened by its intelligent designer.");

  CURRENT_FIELD(codename, std::string);
  CURRENT_FIELD_DESCRIPTION(codename, "The codename of the service instance, assigned randomly at its startup.");

  CURRENT_FIELD(local_port, uint16_t);
  CURRENT_FIELD_DESCRIPTION(local_port, "The local port on which this server is listening.");

  CURRENT_FIELD(cloud_instance_name, Optional<std::string>);
  CURRENT_FIELD_DESCRIPTION(cloud_instance_name, "The name of the instance in the cloud.");

  CURRENT_FIELD(cloud_availability_group, Optional<std::string>);
  CURRENT_FIELD_DESCRIPTION(cloud_availability_group, "The availability group in the cloud.");

  // Dependencies as "ip:port[/prefix]" for now.
  CURRENT_FIELD(dependencies, std::vector<ClaireServiceKey>);
  CURRENT_FIELD_DESCRIPTION(
      dependencies, "The list of dependencies for this service. Will become arrows as the fleet is being visualized.");

  // The `address_port_route` of the currently configured Karl Locator.
  CURRENT_FIELD(reporting_to, std::string);
  CURRENT_FIELD_DESCRIPTION(reporting_to, "The address is used to report keepalives to.");

  CURRENT_FIELD(now, std::chrono::microseconds);  // To calculated time skew as well.
  CURRENT_FIELD_DESCRIPTION(now,
                            "Unix epoch microseconds, local time the keepalive was generated on the machine "
                            "running the service. Used to estimate time skew.");
  CURRENT_FIELD(start_time_epoch_microseconds, std::chrono::microseconds);
  CURRENT_FIELD_DESCRIPTION(start_time_epoch_microseconds,
                            "Unix epoch microseconds from which the uptime of this binary is counted.");
  CURRENT_FIELD(uptime, std::string);
  CURRENT_FIELD_DESCRIPTION(uptime, "The uptime of this service, human-readable.");

  CURRENT_FIELD(last_keepalive_sent, std::string);
  CURRENT_FIELD_DESCRIPTION(last_keepalive_sent, "When was the last keepalive sent, human-readable.");
  CURRENT_FIELD(last_keepalive_status, std::string);
  CURRENT_FIELD_DESCRIPTION(last_keepalive_status, "Whether the last keepalive sent succeeded, human-readable.");
  CURRENT_FIELD(last_successful_keepalive, Optional<std::string>);
  CURRENT_FIELD_DESCRIPTION(last_successful_keepalive,
                            "When did the last successful keepalive happen, human-readable.");
  CURRENT_FIELD(last_successful_keepalive_ping, Optional<std::string>);
  CURRENT_FIELD_DESCRIPTION(last_successful_keepalive_ping,
                            "Ping as measured during the last successful keepalive, human-readable.");
  CURRENT_FIELD(last_successful_keepalive_ping_us, Optional<std::chrono::microseconds>);
  CURRENT_FIELD_DESCRIPTION(last_successful_keepalive_ping_us,
                            "Ping as measured during the last successful keepalive, in microseconds.");

  CURRENT_FIELD(build, Optional<build::BuildInfo>);
  CURRENT_FIELD_DESCRIPTION(build,
                            "The JSON containing the build info collected and imprinted "
                            "into the binary running the service as it was built.");
};

namespace default_user_status {
// The default user-defined status. Wrapped into a namespace for cleaner JSON output.
CURRENT_STRUCT(status) {
  CURRENT_FIELD(message, std::string, "OK");
  CURRENT_FIELD(details, (std::map<std::string, std::string>));

  // LCOV_EXCL_START
  // clang-format off
  void Render(std::ostream& os, std::chrono::microseconds) const {
    os << "<TR><TD COLSPAN='2'>" << message << "</TD></TR>";
    for (const auto& kv : details) {
      os << "<TR><TD>" << kv.first << "</TD><TD>" << kv.second << "</TD></TR>";
    }
  }
  // clang-format on
  // LCOV_EXCL_STOP
};
}  // namespace current::karl::default_user_status

// For now, Karl parses the passed in JSON twice: once as `ClaireStatus` for generic response,
// and once as `ClaireServiceStatus<T>`, where `T` is a `Variant` containing the client-, service-side blob.
// clang-format off
CURRENT_STRUCT_T_DERIVED(ClaireServiceStatus, ClaireStatus) {
  CURRENT_DEFAULT_CONSTRUCTOR_T(ClaireServiceStatus) {}
  CURRENT_CONSTRUCTOR_T(ClaireServiceStatus)(const ClaireStatus& base_status) : SUPER(base_status) {}

  CURRENT_FIELD(runtime, Optional<T>);  // `T` is a `Variant<>` which should be `Optional<>`.
};

// clang-format on

}  // namespace current::karl
}  // namespace current

#endif  // KARL_SCHEMA_CLAIRE_H
