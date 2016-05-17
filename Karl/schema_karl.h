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
  CURRENT_FIELD_DESCRIPTION(
      timestamp,
      "Unix epoch microseconds of the time this report was generated. Can be used to measure time skew.");
  CURRENT_USE_FIELD_AS_KEY(timestamp);

  CURRENT_FIELD(up, bool, true);
  CURRENT_FIELD_DESCRIPTION(up, "`true` for starting up, `false` for a graceful shutdown.");

  CURRENT_FIELD(persisted_keepalives_info, Optional<idxts_t>);
  CURRENT_FIELD_DESCRIPTION(persisted_keepalives_info,
                            "Details on the stream which stores the received keepalives.");

  CURRENT_FIELD(karl_build_info, build::Info, build::Info());
  CURRENT_FIELD_DESCRIPTION(karl_build_info, "Build information of Karl itself.");
};

CURRENT_ENUM(ClaireRegisteredState, uint8_t){Active = 0u, Deregistered = 1u, DisconnectedByTimeout = 2u};

// Per-service static info -- ideally, what changes once during its startup, [ WTF ] but also dependencies.
CURRENT_STRUCT(ClaireInfo) {
  CURRENT_FIELD(codename, std::string);
  CURRENT_FIELD_DESCRIPTION(codename, "The unique codename of an instance of a running service.");
  CURRENT_USE_FIELD_AS_KEY(codename);

  CURRENT_FIELD(service, std::string);
  CURRENT_FIELD_DESCRIPTION(service, "The name of the service.");

  CURRENT_FIELD(location, ClaireServiceKey);
  CURRENT_FIELD_DESCRIPTION(location, "The location of the service.");

  CURRENT_FIELD(reported_timestamp, std::chrono::microseconds);
  CURRENT_FIELD_DESCRIPTION(reported_timestamp,
                            "The time when persisted information about this service has been last updated.");

  CURRENT_FIELD(url_status_page_direct, std::string);
  CURRENT_FIELD_DESCRIPTION(url_status_page_direct,
                            "The direct, world-inaccessible, URL for the status page of this service.");

  CURRENT_FIELD(registered_state, ClaireRegisteredState);
  CURRENT_FIELD_DESCRIPTION(registered_state, "Active = 0, Deregistered = 1, DisconnectedByTimeout = 2.");
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
  CURRENT_FIELD_DESCRIPTION(error, "The error message, human-readable.");
  CURRENT_CONSTRUCTOR(Error)(const std::string& error = "Error.") : error(error) {}
};

CURRENT_STRUCT(up) {
  CURRENT_FIELD(start_time_epoch_microseconds, std::chrono::microseconds);
  CURRENT_FIELD_DESCRIPTION(start_time_epoch_microseconds,
                            "Unix epoch microseconds of the time the service was started.");
  CURRENT_FIELD(last_keepalive_received, std::string);
  CURRENT_FIELD_DESCRIPTION(
      last_keepalive_received,
      "Time elapsed since the most recent keepalive message was received from this service, human-readable.");
  CURRENT_FIELD(last_keepalive_received_epoch_microseconds, std::chrono::microseconds);
  CURRENT_FIELD_DESCRIPTION(
      last_keepalive_received_epoch_microseconds,
      "Unix epoch microseconds of the most recent keepalive message received from this service.");
  CURRENT_FIELD(uptime, std::string);
  CURRENT_FIELD_DESCRIPTION(uptime,
                            "The uptime of the service as of right before it went offline, human-readable.");

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
  CURRENT_FIELD_DESCRIPTION(start_time_epoch_microseconds,
                            "Unix epoch microseconds of the time the service was started.");

  CURRENT_FIELD(last_keepalive_received, std::string);
  CURRENT_FIELD_DESCRIPTION(last_keepalive_received,
                            "Time elapsed ince the most recent keepalive message was received from this "
                            "service before it went offline, human-readable.");
  CURRENT_FIELD(last_keepalive_received_epoch_microseconds, std::chrono::microseconds);
  CURRENT_FIELD_DESCRIPTION(last_keepalive_received_epoch_microseconds,
                            "Unix epoch microseconds of the most recent keepalive message received from this "
                            "service before it went offline.");
  CURRENT_FIELD(last_confirmed_uptime, std::string);
  CURRENT_FIELD_DESCRIPTION(last_confirmed_uptime,
                            "The uptime of the service as of right before it went offline, human-readable.");

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
  CURRENT_FIELD_DESCRIPTION(currently, "`up` or `down`, with respective details.");

  CURRENT_FIELD(service, std::string);
  CURRENT_FIELD_DESCRIPTION(service, "The service name.");

  CURRENT_FIELD(codename, std::string);
  CURRENT_FIELD_DESCRIPTION(
      codename, "The codename of this instance of the running service, assigned randomly at its startup.");

  CURRENT_FIELD(location, ClaireServiceKey);
  CURRENT_FIELD_DESCRIPTION(location,
                            "The information identifying where is this service running and how to reach it.");

  CURRENT_FIELD(dependencies, std::vector<std::string>);
  CURRENT_FIELD_DESCRIPTION(dependencies, "Other services this one depends on, resolved to their codenames.");

  CURRENT_FIELD(unresolved_dependencies, std::vector<std::string>);
  CURRENT_FIELD_DESCRIPTION(unresolved_dependencies,
                            "Other services this one depends on, which can not be resolved into codenames, and "
                            "thus are reported as the URLs of their internal status pages.");

  CURRENT_FIELD(url_status_page_direct, std::string);
  CURRENT_FIELD_DESCRIPTION(url_status_page_direct,
                            "The direct, internal, URL to access the status page of this service.");

  CURRENT_FIELD(url_status_page_proxied, Optional<std::string>);
  CURRENT_FIELD_DESCRIPTION(url_status_page_proxied,
                            "The proxied, external, URL to access to status page of this service.");

  CURRENT_FIELD(build_time, std::string);
  CURRENT_FIELD_DESCRIPTION(build_time,
                            "An exempt of this service's build information, to help visually locating a "
                            "specific, usually recently started, service.");
  CURRENT_FIELD(build_time_epoch_microseconds, std::chrono::microseconds);
  CURRENT_FIELD_DESCRIPTION(build_time_epoch_microseconds,
                            "An exempt of this service's build information, to help visually locating a "
                            "specific, usually recently started, service.");
  CURRENT_FIELD(git_commit, std::string);
  CURRENT_FIELD_DESCRIPTION(git_commit,
                            "An exempt of this service's build information, to help visually locating a "
                            "specific, usually recently started, service.");
  CURRENT_FIELD(git_branch, std::string);
  CURRENT_FIELD_DESCRIPTION(git_branch,
                            "An exempt of this service's build information, to help visually locating a "
                            "specific, usually recently started, service.");
  CURRENT_FIELD(git_dirty, bool);
  CURRENT_FIELD_DESCRIPTION(git_dirty,
                            "An exempt of this service's build information, to help visually locating a "
                            "specific, usually recently started, service.");

  CURRENT_FIELD(runtime, Optional<T>);
  CURRENT_FIELD_DESCRIPTION(runtime, "The service-specific runtime information about this service.");
};

CURRENT_STRUCT_T(ServerToReport) {
  CURRENT_FIELD(time_skew, std::string);
  CURRENT_FIELD_DESCRIPTION(
      time_skew,
      "Whether this server is on par, ahead, or behind the server that generated this report local-time-wise.");
  CURRENT_FIELD(services, (std::map<std::string, ServiceToReport<T>>));
  CURRENT_FIELD_DESCRIPTION(services, "The list of services running on this server.");
};

CURRENT_STRUCT_T(GenericKarlStatus) {
  CURRENT_FIELD(now, std::chrono::microseconds);
  CURRENT_FIELD_DESCRIPTION(now, "Unix epoch microseconds of when this report has been generated.");
  CURRENT_FIELD(from, std::chrono::microseconds);
  CURRENT_FIELD_DESCRIPTION(from,
                            "Unix epoch microseconds of the beginning of the time range this report covers.");
  CURRENT_FIELD(to, std::chrono::microseconds);
  CURRENT_FIELD_DESCRIPTION(to, "Unix epoch microseconds of the end of the time range this report covers.");
  CURRENT_FIELD(generation_time, std::chrono::microseconds);
  CURRENT_FIELD_DESCRIPTION(generation_time,
                            "How long did the generation of this report take, in microseconds.");
  CURRENT_FIELD(machines, (std::map<std::string, ServerToReport<T>>));
  CURRENT_FIELD_DESCRIPTION(machines, "The actual report, grouped by servers.");
};

CURRENT_STRUCT_T(SnapshotOfKeepalive) {
  CURRENT_FIELD(age, std::string);
  CURRENT_FIELD_DESCRIPTION(age, "How long ago was this snapshot saved, human-readable.");

  CURRENT_FIELD(snapshot, ClaireServiceStatus<T>);
  CURRENT_FIELD_DESCRIPTION(snapshot, "The snapshot itself.");

  CURRENT_CONSTRUCTOR_T(SnapshotOfKeepalive)(std::chrono::microseconds dt,
                                             const ClaireServiceStatus<T>& data = ClaireServiceStatus<T>())
      : age(strings::TimeDifferenceAsHumanReadableString(dt)), snapshot(data) {}
};

}  // namespace current::karl
}  // namespace current

#endif  // KARL_SCHEMA_KARL_H
