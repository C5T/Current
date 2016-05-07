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

// Karl is the module responsible for collecting keepalives from Claire-s and reporting/visualizing them.
//
// Karl's storage model contains of the following pieces:
//
// 1) The Sherlock `Stream` of all keepalives received. Persisted on disk, not stored in memory.
//    Each "visualize production" request (be it JSON or SVG response) replays that stream over the desired
//    period of time. Most commonly it's the past five minutes.
//
// 2) The `Storage`, over a separate stream, to retain the information which may be required outside the
//    "visualized" time window. Includes Karl's launch history, and per-service codename -> build::Info.
//
// TODO(dkorolev) + TODO(mzhurovich): Stuff like nginx config for fresh start lives in the Storage part, right?
//                                    We'll need to have GenericKarl accept custom storage type then.
//
// The conventional wisdom is that Karl can start with both 1) and 2) missing. After one keepalive cycle,
// which is under half a minute, it would regain the state of the fleet, as long as all keepalives go to it.

// NOTE: Local `current_build.h` must be included before Karl/Claire headers.

#ifndef KARL_KARL_H
#define KARL_KARL_H

#include "../port.h"

#include "schema.h"
#include "locator.h"

#include "../Storage/storage.h"
#include "../Storage/persister/sherlock.h"

#include "../Bricks/net/http/impl/server.h"

#include "../Blocks/HTTP/api.h"

#ifdef EXTRA_KARL_LOGGING
#include "../TypeSystem/Schema/schema.h"
#endif

namespace current {
namespace karl {

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Karl's persisted storage schema.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

  CURRENT_FIELD(url_status_page_proxied, std::string);
  CURRENT_FIELD(url_status_page_direct, std::string);

  CURRENT_FIELD(service, std::string);
  CURRENT_FIELD(location, ClaireServiceKey);

  CURRENT_FIELD(reported_timestamp, std::chrono::microseconds);
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Karl's HTTP response(s) schema.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace current_service_state {

CURRENT_STRUCT(up) {
  CURRENT_FIELD(last_keepalive_received, std::string);
  CURRENT_FIELD(uptime, std::string);

  CURRENT_DEFAULT_CONSTRUCTOR(up) {}
  CURRENT_CONSTRUCTOR(up)(const std::string& last_keepalive_received, const std::string& uptime)
      : last_keepalive_received(last_keepalive_received), uptime(uptime) {}
};

CURRENT_STRUCT(down) {
  CURRENT_FIELD(last_keepalive_received, std::string);
  CURRENT_FIELD(last_confirmed_uptime, std::string);

  CURRENT_DEFAULT_CONSTRUCTOR(down) {}
  CURRENT_CONSTRUCTOR(down)(const std::string& last_keepalive_received,
                            const std::string& last_confirmed_uptime)
      : last_keepalive_received(last_keepalive_received), last_confirmed_uptime(last_confirmed_uptime) {}
};

using state_variant_t = Variant<up, down>;

}  // namespace current::karl::current_service_state

CURRENT_STRUCT_T(ServiceToReport) {
  CURRENT_FIELD(currently, current_service_state::state_variant_t);
  CURRENT_FIELD(service, std::string);
  CURRENT_FIELD(codename, std::string);
  CURRENT_FIELD(location, ClaireServiceKey);
  CURRENT_FIELD(dependencies, std::vector<std::string>);             // Codenames.
  CURRENT_FIELD(unresolved_dependencies, std::vector<std::string>);  // Status page URLs.
  CURRENT_FIELD(url_status_page_proxied, std::string);
  CURRENT_FIELD(url_status_page_direct, std::string);
  CURRENT_FIELD(runtime, Optional<T>);
};

CURRENT_STRUCT_T(ServerToReport) {
  CURRENT_FIELD(behind_this_by, std::string);
  CURRENT_FIELD(services, (std::map<std::string, ServiceToReport<T>>));
};

template <typename RUNTIME_STATUS_VARIANT>
using GenericKarlStatus = std::map<std::string, ServerToReport<RUNTIME_STATUS_VARIANT>>;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Karl's implementation.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CURRENT_STRUCT_T(KarlPersistedKeepalive) {
  CURRENT_FIELD(location, ClaireServiceKey);
  CURRENT_FIELD(keepalive, T);
};

template <typename... TS>
class GenericKarl final {
 public:
  using runtime_status_variant_t = Variant<TS...>;
  using claire_status_t = ClaireServiceStatus<runtime_status_variant_t>;
  using karl_status_t = GenericKarlStatus<runtime_status_variant_t>;
  using persisted_keepalive_t = KarlPersistedKeepalive<claire_status_t>;
  using stream_t = sherlock::Stream<persisted_keepalive_t, current::persistence::File>;
  using storage_t = ServiceStorage<SherlockStreamPersister>;

  explicit GenericKarl(uint16_t port,
                       const std::string& stream_persistence_file,
                       const std::string& storage_persistence_file,
                       const std::string& url = "/",
                       const std::string& external_url = "http://localhost:7576/",
                       std::chrono::microseconds up_threshold = std::chrono::microseconds(1000ll * 1000ll * 45))
      : external_url_(external_url),
        up_threshold_(up_threshold),
        keepalives_stream_(stream_persistence_file),
        storage_(storage_persistence_file),
        http_scope_(HTTP(port).Register(url, [this](Request r) { return Serve(std::move(r)); })) {
    // Report this Karl as up and running.
    // Oh, look, I'm doing work in constructor body. Sigh. -- D.K.
    storage_.ReadWriteTransaction([this](MutableFields<storage_t> fields) {
      const auto& stream_persister = keepalives_stream_.InternalExposePersister();
      KarlInfo self_info;
      if (!stream_persister.Empty()) {
        self_info.persisted_keepalives_info = stream_persister.LastPublishedIndexAndTimestamp();
      }

      fields.karl.Add(self_info);
    }).Wait();
  }

  ~GenericKarl() {
    storage_.ReadWriteTransaction([this](MutableFields<storage_t> fields) {
      KarlInfo self_info;
      self_info.up = false;
      fields.karl.Add(self_info);
    }).Wait();
  }

 private:
  void Serve(Request r) {
    if (r.method != "GET" && r.method != "POST" && r.method != "DELETE") {
      r(current::net::DefaultMethodNotAllowedMessage(), HTTPResponseCode.MethodNotAllowed, "text/html");
      return;
    }

    const auto& qs = r.url.query;

    if (r.method == "DELETE") {
      const std::string ip = r.connection.RemoteIPAndPort().ip;
      if (qs.has("codename")) {
        const std::string codename = qs["codename"];
        storage_.ReadWriteTransaction([codename](MutableFields<storage_t> fields) -> Response {
          ClaireInfo claire;
          const auto& current_claire_info = fields.claires[codename];
          if (Exists(current_claire_info)) {
            claire = Value(current_claire_info);
          } else {
            claire.codename = codename;
          }
          claire.registered_state = ClaireRegisteredState::Deregistered;
          fields.claires.Add(claire);
          return Response("OK\n");
        }, std::move(r)).Detach();
      } else {
        // Respond with "200 OK" in any case.
        r("NOP\n");
      }
    } else if (r.method == "POST") {
      try {
        const std::string ip = r.connection.RemoteIPAndPort().ip;
        const std::string url = "http://" + ip + ':' + qs["port"] + "/.current";

        // If `&confirm` is set, along with `codename` and `port`, Karl calls the service back
        // via the URL from the inbound request and the port the service has provided,
        // to confirm two-way communication.
        const std::string json = [&]() -> std::string {
          if (qs.has("confirm") && qs.has("port")) {
            // Send a GET request, with a random component in the URL to prevent caching.
            return HTTP(GET(url + "?all&rnd" + current::ToString(current::random::CSRandomUInt(1e9, 2e9))))
                .body;
          } else {
            return r.body;
          }
        }();

        const auto body = ParseJSON<ClaireStatus>(json);
        if ((!qs.has("codename") || body.codename == qs["codename"]) &&
            (!qs.has("port") || body.local_port == current::FromString<uint16_t>(qs["port"]))) {
          ClaireServiceKey location;
          location.ip = ip;
          location.port = body.local_port;
          location.prefix = "/";  // TODO(dkorolev) + TODO(mzhurovich): Add support for `qs["prefix"]`.

          const auto dependencies = body.dependencies;

          // If the received status can be parsed in detail, including the "runtime" variant, persist it.
          // If no, no big deal, keep the top-level one regardless.
          const auto status = [&]() -> claire_status_t {
            try {
              return ParseJSON<claire_status_t>(json);
            } catch (const TypeSystemParseJSONException&) {

#ifdef EXTRA_KARL_LOGGING
              std::cerr << "Could not parse: " << json << '\n';
              reflection::StructSchema struct_schema;
              struct_schema.AddType<claire_status_t>();
              std::cerr << "As:\n" << struct_schema.GetSchemaInfo().Describe<reflection::Language::Current>()
                        << '\n';
#endif

              claire_status_t status;
              // Initialize `ClaireStatus` from `ClaireServiceStatus`, keep the `Variant<...> runtime` empty.
              static_cast<ClaireStatus&>(status) = body;
              return status;
            }
          }();

          {
            persisted_keepalive_t record;
            record.location = location;
            record.keepalive = status;
            keepalives_stream_.Publish(std::move(record));
          }

          const auto now = current::time::Now();
          const std::string service = body.service;
          const std::string codename = body.codename;

          Optional<current::build::Info> optional_build = body.build;
          Optional<std::chrono::microseconds> optional_behind_this_by;
          if (Exists(body.last_successful_ping_epoch_microseconds)) {
            optional_behind_this_by = now - body.now - Value(body.last_successful_ping_epoch_microseconds) / 2;
          }

          storage_.ReadWriteTransaction(
                       [this,
                        now,
                        codename,
                        service,
                        location,
                        dependencies,
                        optional_build,
                        optional_behind_this_by](MutableFields<storage_t> fields) -> Response {
                         // Update per-server time skew.
                         if (Exists(optional_behind_this_by)) {
                           const std::chrono::microseconds behind_this_by = Value(optional_behind_this_by);
                           ServerInfo server;
                           server.ip = location.ip;
                           // Just in case load old value if we decide to add fields to `ServerInfo`.
                           const ImmutableOptional<ServerInfo> current_server_info =
                               fields.servers[location.ip];
                           bool need_to_update = true;
                           if (Exists(current_server_info)) {
                             server = Value(current_server_info);
                             const auto time_skew_difference = server.behind_this_by - behind_this_by;
                             if (static_cast<uint64_t>(std::abs(time_skew_difference.count())) <
                                 kUpdateServerInfoThresholdByTimeSkewDifference) {
                               need_to_update = false;
                             }
                           }
                           if (need_to_update) {
                             server.behind_this_by = behind_this_by;
                             fields.servers.Add(server);
                           }
                         }

                         // Update the `DB` if the build information was not stored there yet.
                         const ImmutableOptional<ClaireBuildInfo> current_claire_build_info =
                             fields.builds[codename];
                         if (!Exists(current_claire_build_info) ||
                             (Exists(optional_build) &&
                              Value(current_claire_build_info).build != Value(optional_build))) {
                           ClaireBuildInfo build;
                           build.codename = codename;
                           build.build = Value(optional_build);
                           fields.builds.Add(build);
                         }
                         // Update the `DB` if "codename", "location", or "dependencies" differ.
                         const ImmutableOptional<ClaireInfo> current_claire_info = fields.claires[codename];
                         if ([&]() {
                               if (!Exists(current_claire_info)) {
                                 return true;
                               } else if (Value(current_claire_info).location != location) {
                                 return true;
                               } else {
                                 return false;
                               }
                             }()) {
                           ClaireInfo claire;
                           if (Exists(current_claire_info)) {
                             // Do not overwrite `build` with `null`.
                             claire = Value(current_claire_info);
                           }
                           claire.codename = codename;

                           // TODO(mzhurovich): This one should work via `nginx`, I'd assume.
                           claire.url_status_page_proxied = external_url_ + "proxied/" + codename;
                           claire.url_status_page_direct = location.StatusPageURL();

                           claire.service = service;
                           claire.location = location;

                           claire.reported_timestamp = now;
                           claire.registered_state = ClaireRegisteredState::Active;

                           fields.claires.Add(claire);
                         }
                         return Response("OK\n");
                       },
                       std::move(r)).Detach();
        } else {
          r("Inconsistent URL/body parameters.\n", HTTPResponseCode.BadRequest);
        }
      } catch (const net::NetworkException&) {
        r("Callback error.\n", HTTPResponseCode.BadRequest);
      } catch (const TypeSystemParseJSONException&) {
        r("JSON parse error.\n", HTTPResponseCode.BadRequest);
      } catch (const Exception&) {
        r("Karl registration error.\n", HTTPResponseCode.InternalServerError);
      }
    } else {
      BuildStatusAndRespondWithIt(std::move(r));
    }
  }

  void BuildStatusAndRespondWithIt(Request r) {
    // Non-POST, a.k.a. GET.
    const auto now = current::time::Now();
    const auto from = [&]() -> std::chrono::microseconds {
      if (r.url.query.has("from")) {
        return current::FromString<std::chrono::microseconds>(r.url.query["from"]);
      }
      if (r.url.query.has("m")) {  // `m` stands for minutes.
        return now - std::chrono::microseconds(
                         static_cast<int64_t>(current::FromString<double>(r.url.query["m"]) * 1e6 * 60));
      }
      if (r.url.query.has("h")) {  // `h` stands for hours.
        return now - std::chrono::microseconds(
                         static_cast<int64_t>(current::FromString<double>(r.url.query["h"]) * 1e6 * 60 * 60));
      }
      if (r.url.query.has("d")) {  // `d` stands for days.
        return now - std::chrono::microseconds(static_cast<int64_t>(
                         current::FromString<double>(r.url.query["d"]) * 1e6 * 60 * 60 * 24));
      }
      // Five minutes by default.
      return now - std::chrono::microseconds(static_cast<int64_t>(1e6 * 60 * 5));
    }();
    const auto to = [&]() -> std::chrono::microseconds {
      if (r.url.query.has("to")) {
        return current::FromString<std::chrono::microseconds>(r.url.query["to"]);
      }
      if (r.url.query.has("interval_us")) {
        return from + current::FromString<std::chrono::microseconds>(r.url.query["interval_us"]);
      }
      // By the present moment by default.
      return now;
    }();

    // Codenames to resolve to `ClaireServiceKey`-s later, in a `ReadOnlyTransaction`.
    std::unordered_set<std::string> codenames_to_resolve;

    // The builder for the response.
    struct ProtoReport {
      current_service_state::state_variant_t currently;
      std::vector<ClaireServiceKey> dependencies;
      Optional<runtime_status_variant_t> runtime;  // Must be `Optional<>`, as it is in `ClaireServiceStatus`.
    };
    std::map<std::string, ProtoReport> report_for_codename;
    std::map<std::string, std::set<std::string>> codenames_per_service;
    std::map<ClaireServiceKey, std::string> service_key_into_codename;

    for (const auto& e : keepalives_stream_.InternalExposePersister().Iterate()) {
      if (e.idx_ts.us >= from && e.idx_ts.us < to) {
        const claire_status_t keepalive = e.entry.keepalive;

        codenames_to_resolve.insert(keepalive.codename);
        service_key_into_codename[e.entry.location] = keepalive.codename;

        codenames_per_service[keepalive.service].insert(keepalive.codename);
        // DIMA: More per-codename reporting fields go here; tailored to specific type, `.Call(populator)`, etc.
        ProtoReport report;
        const std::string last_keepalive =
            current::strings::TimeIntervalAsHumanReadableString(now - e.idx_ts.us) + " ago";
        if ((now - e.idx_ts.us) < up_threshold_) {
          // Service is up.
          const auto projected_uptime_us = keepalive.uptime_epoch_microseconds + (now - e.idx_ts.us);
          report.currently = current_service_state::up(
              last_keepalive, current::strings::TimeIntervalAsHumanReadableString(projected_uptime_us));
        } else {
          // Service is down.
          report.currently = current_service_state::down(last_keepalive, keepalive.uptime);
        }
        report.dependencies = keepalive.dependencies;
        report.runtime = keepalive.runtime;
        report_for_codename[keepalive.codename] = report;
      }
    }

    // To avoid `JSONFormat::Minimalistic`, at least for unit tests.
    const bool full_format = r.url.query.has("full");
    // To list only the services that are currently in `Active` state.
    const bool active_only = r.url.query.has("active_only");

    storage_.ReadOnlyTransaction(
                 [this,
                  full_format,
                  active_only,
                  codenames_to_resolve,
                  report_for_codename,
                  codenames_per_service,
                  service_key_into_codename](ImmutableFields<storage_t> fields) -> Response {
                   std::unordered_map<std::string, ClaireServiceKey> resolved_codenames;
                   karl_status_t result;
                   for (const auto& codename : codenames_to_resolve) {
                     resolved_codenames[codename] = [&]() -> ClaireServiceKey {
                       const ImmutableOptional<ClaireInfo> resolved = fields.claires[codename];
                       if (Exists(resolved)) {
                         return Value(resolved).location;
                       } else {
                         ClaireServiceKey key;
                         key.ip = "zombie/" + codename;
                         key.port = 0;
                         return key;
                       }
                     }();
                   }
                   for (const auto& iterating_over_services : codenames_per_service) {
                     const std::string& service = iterating_over_services.first;
                     for (const auto& codename : iterating_over_services.second) {
                       ServiceToReport<runtime_status_variant_t> blob;
                       const auto& rhs = report_for_codename.at(codename);
                       if (active_only) {
                         const auto& persisted_claire = fields.claires[codename];
                         if (Exists(persisted_claire) &&
                             Value(persisted_claire).registered_state != ClaireRegisteredState::Active) {
                           continue;
                         }
                       }
                       blob.currently = rhs.currently;
                       blob.service = service;
                       blob.codename = codename;
                       blob.location = resolved_codenames[codename];
                       for (const auto& dep : rhs.dependencies) {
                         const auto cit = service_key_into_codename.find(dep);
                         if (cit != service_key_into_codename.end()) {
                           blob.dependencies.push_back(cit->second);
                         } else {
                           blob.unresolved_dependencies.push_back(dep.StatusPageURL());
                         }
                       }
                       blob.url_status_page_proxied = external_url_ + "proxied/" + codename;
                       blob.url_status_page_direct = blob.location.StatusPageURL();
                       blob.location = resolved_codenames[codename];
                       blob.runtime = rhs.runtime;
                       result[blob.location.ip].services[codename] = std::move(blob);
                     }
                   }
                   // Update per-server time skew information.
                   for (auto& iterating_over_reported_servers : result) {
                     const std::string& ip = iterating_over_reported_servers.first;
                     auto& server = iterating_over_reported_servers.second;
                     const auto persisted_server_info = fields.servers[ip];
                     if (Exists(persisted_server_info)) {
                       const int64_t behind_this_by_us = Value(persisted_server_info).behind_this_by.count();
                       if (std::abs(behind_this_by_us) < 100000) {
                         server.behind_this_by = "Less than +/-0.1s";
                       } else {
                         server.behind_this_by = current::strings::Printf("%.1lfs", 1e-6 * behind_this_by_us);
                       }
                     }
                   }
                   if (full_format) {
                     return result;
                   } else {
                     return Response(JSON<JSONFormat::Minimalistic>(result),
                                     HTTPResponseCode.OK,
                                     current::net::constants::kDefaultJSONContentType);
                   }
                 },
                 std::move(r)).Detach();
  }

  const std::string external_url_;
  const std::chrono::microseconds up_threshold_;
  stream_t keepalives_stream_;
  storage_t storage_;
  const HTTPRoutesScope http_scope_;
};

using Karl = GenericKarl<default_user_status::status>;

}  // namespace current::karl
}  // namespace current

#endif  // KARL_KARL_H
