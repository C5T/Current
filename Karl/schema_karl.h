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

#ifndef KARL_SCHEMA_KARL_H
#define KARL_SCHEMA_KARL_H

#include "../port.h"

#include "schema_claire.h"
#include "locator.h"

#include "../Storage/storage.h"

namespace current {
namespace karl {

// Karl's persisted storage schema.

// System info, startup/teardown.
CURRENT_STRUCT(KarlInfo) {
  CURRENT_FIELD(timestamp, std::chrono::microseconds, current::time::Now());
  CURRENT_USE_FIELD_AS_KEY(timestamp);

  // `true` for starting up, `false` for a graceful shutdown.
  CURRENT_FIELD(up, bool, true);

  // The information on the `Stream` of keepalives received, as seen by Karl upon starting up or tearing down.
  CURRENT_FIELD(persisted_keepalives_info, Optional<idxts_t>);

  // The `build::Info` of this Karl itself.
  CURRENT_FIELD(karl_build_info, build::Info, build::Info());
};

CURRENT_ENUM(ClaireRegisteredState, uint8_t){Active = 0u, Deregistered = 1u, DisconnectedByTimeout = 2u};

// Per-service static info -- ideally, what changes once during its startup, [ WTF ] but also dependencies.
CURRENT_STRUCT(ClaireInfo) {
  CURRENT_FIELD(codename, std::string);
  CURRENT_USE_FIELD_AS_KEY(codename);

  CURRENT_FIELD(service, std::string);
  CURRENT_FIELD(location, ClaireServiceKey);
  CURRENT_FIELD(reported_timestamp, std::chrono::microseconds);
  CURRENT_FIELD(url_status_page_direct, std::string);
  CURRENT_FIELD(registered_state, ClaireRegisteredState);
};

CURRENT_STRUCT(ClaireBuildInfo) {
  CURRENT_FIELD(codename, std::string);
  CURRENT_USE_FIELD_AS_KEY(codename);

  CURRENT_FIELD(build, current::build::Info);
};

const uint64_t kUpdateServerInfoThresholdByTimeSkewDifference = 50000;

// Per-server status info.
CURRENT_STRUCT(ServerInfo) {
  CURRENT_FIELD(ip, std::string);
  CURRENT_USE_FIELD_AS_KEY(ip);

  CURRENT_FIELD(behind_this_by, std::chrono::microseconds);
};

CURRENT_STORAGE_FIELD_ENTRY(OrderedDictionary, KarlInfo, KarlInfoDictionary);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedDictionary, ClaireInfo, ClaireInfoDictionary);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedDictionary, ClaireBuildInfo, BuildInfoDictionary);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedDictionary, ServerInfo, ServerInfoDictionary);

CURRENT_STORAGE(ServiceStorage) {
  CURRENT_STORAGE_FIELD(karl, KarlInfoDictionary);
  CURRENT_STORAGE_FIELD(claires, ClaireInfoDictionary);
  CURRENT_STORAGE_FIELD(builds, BuildInfoDictionary);
  CURRENT_STORAGE_FIELD(servers, ServerInfoDictionary);
};

// Karl's HTTP response(s) schema.
namespace current_service_state {

CURRENT_STRUCT(Error) {
  CURRENT_FIELD(error, std::string);
  CURRENT_CONSTRUCTOR(Error)(const std::string& error = "Error.") : error(error) {}
};

CURRENT_STRUCT(up) {
  CURRENT_FIELD(start_time_epoch_microseconds, std::chrono::microseconds);
  CURRENT_FIELD(last_keepalive_received, std::string);
  CURRENT_FIELD(last_keepalive_received_epoch_microseconds, std::chrono::microseconds);
  CURRENT_FIELD(uptime, std::string);

  CURRENT_DEFAULT_CONSTRUCTOR(up) {}
  CURRENT_CONSTRUCTOR(up)(const std::chrono::microseconds start_time_epoch_microseconds,
                          const std::string& last_keepalive_received,
                          const std::chrono::microseconds last_keepalive_received_epoch_microseconds,
                          const std::string& uptime)
      : start_time_epoch_microseconds(start_time_epoch_microseconds),
        last_keepalive_received(last_keepalive_received),
        last_keepalive_received_epoch_microseconds(last_keepalive_received_epoch_microseconds),
        uptime(uptime) {}
};

CURRENT_STRUCT(down) {
  CURRENT_FIELD(start_time_epoch_microseconds, std::chrono::microseconds);
  CURRENT_FIELD(last_keepalive_received, std::string);
  CURRENT_FIELD(last_keepalive_received_epoch_microseconds, std::chrono::microseconds);
  CURRENT_FIELD(last_confirmed_uptime, std::string);

  CURRENT_DEFAULT_CONSTRUCTOR(down) {}
  CURRENT_CONSTRUCTOR(down)(const std::chrono::microseconds start_time_epoch_microseconds,
                            const std::string& last_keepalive_received,
                            const std::chrono::microseconds last_keepalive_received_epoch_microseconds,
                            const std::string& last_confirmed_uptime)
      : start_time_epoch_microseconds(start_time_epoch_microseconds),
        last_keepalive_received(last_keepalive_received),
        last_keepalive_received_epoch_microseconds(last_keepalive_received_epoch_microseconds),
        last_confirmed_uptime(last_confirmed_uptime) {}
};

// TODO(dkorolev): `Variant<up, down, done>`, where `done` == "has been shut down gracefully".
using state_variant_t = Variant<up, down>;

}  // namespace current::karl::current_service_state

CURRENT_STRUCT_T(ServiceToReport) {
  CURRENT_FIELD(currently, current_service_state::state_variant_t);
  CURRENT_FIELD(service, std::string);
  CURRENT_FIELD(codename, std::string);
  CURRENT_FIELD(location, ClaireServiceKey);
  CURRENT_FIELD(dependencies, std::vector<std::string>);             // Codenames.
  CURRENT_FIELD(unresolved_dependencies, std::vector<std::string>);  // Status page URLs.
  CURRENT_FIELD(url_status_page_direct, std::string);
  CURRENT_FIELD(url_status_page_proxied, Optional<std::string>);
  CURRENT_FIELD(build_time, std::string);
  CURRENT_FIELD(build_time_epoch_microseconds, std::chrono::microseconds);
  CURRENT_FIELD(git_commit, std::string);
  CURRENT_FIELD(git_branch, std::string);
  CURRENT_FIELD(git_dirty, bool);
  CURRENT_FIELD(runtime, Optional<T>);
};

CURRENT_STRUCT_T(ServerToReport) {
  CURRENT_FIELD(time_skew, std::string);
  CURRENT_FIELD(services, (std::map<std::string, ServiceToReport<T>>));
};

CURRENT_STRUCT_T(GenericKarlStatus) {
  CURRENT_FIELD(now, std::chrono::microseconds);
  CURRENT_FIELD(from, std::chrono::microseconds);
  CURRENT_FIELD(to, std::chrono::microseconds);
  CURRENT_FIELD(generation_time, std::chrono::microseconds);
  CURRENT_FIELD(machines, (std::map<std::string, ServerToReport<T>>));
};

CURRENT_STRUCT_T(SnapshotOfKeepalive) {
  CURRENT_FIELD(age, std::string);
  CURRENT_FIELD(snapshot, ClaireServiceStatus<T>);
  CURRENT_CONSTRUCTOR_T(SnapshotOfKeepalive)(std::chrono::microseconds dt,
                                             const ClaireServiceStatus<T>& data = ClaireServiceStatus<T>())
      : age(strings::TimeDifferenceAsHumanReadableString(dt)), snapshot(data) {}
};

}  // namespace current::karl
}  // namespace current

#endif  // KARL_SCHEMA_KARL_H
