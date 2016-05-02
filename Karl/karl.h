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

#include "current_build.h"

#include "schema.h"
#include "locator.h"

#include "../Storage/storage.h"
#include "../Storage/persister/sherlock.h"

#include "../Blocks/HTTP/api.h"

namespace current {
namespace karl {

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Karl's persisted storage schema.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

CURRENT_STRUCT(ClaireInfo) {
  CURRENT_FIELD(codename, std::string);
  CURRENT_USE_FIELD_AS_KEY(codename);

  CURRENT_FIELD(url_status_page_proxied, std::string);
  CURRENT_FIELD(url_status_page_direct, std::string);

  CURRENT_FIELD(service, std::string);
  CURRENT_FIELD(location, ClaireServiceKey);
  CURRENT_FIELD(dependencies, std::vector<ClaireServiceKey>);

  CURRENT_FIELD(build, current::build::Info);
  CURRENT_FIELD(reported_timestamp, std::chrono::microseconds);
};

CURRENT_STORAGE_FIELD_ENTRY(OrderedDictionary, KarlInfo, KarlInfoDictionary);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedDictionary, ClaireInfo, ClaireInfoDictionary);

CURRENT_STORAGE(ServiceStorage) {
  CURRENT_STORAGE_FIELD(karl, KarlInfoDictionary);
  CURRENT_STORAGE_FIELD(claires, ClaireInfoDictionary);
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Karl's HTTP response(s) schema.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CURRENT_STRUCT(ServiceToReport) {
  CURRENT_FIELD(up, bool);
  CURRENT_FIELD(service, std::string);
  CURRENT_FIELD(codename, std::string);
  CURRENT_FIELD(location, ClaireServiceKey);
  CURRENT_FIELD(dependencies, std::vector<std::string>);             // Codenames.
  CURRENT_FIELD(unresolved_dependencies, std::vector<std::string>);  // Status page URLs.
  CURRENT_FIELD(url_status_page_proxied, std::string);
  CURRENT_FIELD(url_status_page_direct, std::string);
  CURRENT_FIELD(uptime_as_of_last_keepalive, std::string);
};

using KarlStatus = std::map<std::string, std::map<std::string, ServiceToReport>>;

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
  using claire_status_t = ClaireServiceStatus<Variant<TS...>>;
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
    if (r.method == "POST") {
      const auto& qs = r.url.query;
      // If `&confirm` is set, along with `codename` and `port`, Karl calls the service back
      // via the URL from the inbound request and the port the service has provided,
      // to confirm two-way communication.
      try {
        const std::string ip = r.connection.RemoteIPAndPort().ip;
        const std::string url = "http://" + ip + ':' + qs["port"] + "/.current";

        const std::string json = [&]() -> std::string {
          if (r.method == "POST" && qs.has("confirm") && qs.has("port")) {
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
            keepalives_stream_.Publish(record);
          }

          const auto now = current::time::Now();
          const std::string service = body.service;
          const std::string codename = body.codename;

          Optional<current::build::Info> optional_build = body.build;

          storage_.ReadWriteTransaction(
                       [this, now, codename, service, location, dependencies, optional_build](
                           MutableFields<storage_t> fields) -> Response {
                         // Update the `DB` if "codename", "location", or "dependencies" differ.
                         const ImmutableOptional<ClaireInfo> current_claire_info = fields.claires[codename];
                         if ([&]() {
                               if (!Exists(current_claire_info)) {
                                 return true;
                               } else if (Exists(optional_build) &&
                                          Value(current_claire_info).build != Value(optional_build)) {
                                 return true;
                               } else if (Value(current_claire_info).location != location) {
                                 return true;
                               } else if (Value(current_claire_info).dependencies != dependencies) {
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
                           claire.dependencies = dependencies;

                           if (Exists(optional_build)) {
                             claire.build = Value(optional_build);
                           }

                           claire.reported_timestamp = now;

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
      bool up;
      std::string uptime;
      std::vector<ClaireServiceKey> dependencies;
    };
    std::map<std::string, ProtoReport> report_for_codename;
    std::map<std::string, std::set<std::string>> codenames_per_service;
    std::map<ClaireServiceKey, std::string> service_key_into_codename;

    for (const auto& e : keepalives_stream_.InternalExposePersister().Iterate()) {
      if (e.idx_ts.us >= from && e.idx_ts.us < to) {
        const auto& keepalive = e.entry.keepalive;

        codenames_to_resolve.insert(keepalive.codename);
        service_key_into_codename[e.entry.location] = keepalive.codename;

        codenames_per_service[keepalive.service].insert(keepalive.codename);
        // DIMA: More per-codename reporting fields go here; tailored to specific type, `.Call(populator)`, etc.
        ProtoReport report;
        report.up = (now - e.idx_ts.us) < up_threshold_;
        report.uptime = keepalive.uptime + ", reported " +
                        current::strings::TimeIntervalAsHumanReadableString(now - e.idx_ts.us) + " ago";
        report.dependencies = keepalive.dependencies;
        report_for_codename[keepalive.codename] = report;
      }
    }

    storage_.ReadOnlyTransaction(
                 [this,
                  codenames_to_resolve,
                  report_for_codename,
                  codenames_per_service,
                  service_key_into_codename](ImmutableFields<storage_t> fields) -> Response {
                   std::unordered_map<std::string, ClaireServiceKey> resolved_codenames;
                   KarlStatus result;
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
                       ServiceToReport blob;
                       const auto& rhs = report_for_codename.at(codename);
                       blob.up = rhs.up;
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
                       // DIMA: More per-codename reporting fields go here.
                       blob.location = resolved_codenames[codename];
                       blob.uptime_as_of_last_keepalive = rhs.uptime;
                       result[blob.location.ip][codename] = std::move(blob);
                     }
                   }
                   return result;
                 },
                 std::move(r)).Detach();
  }

  const std::string external_url_;
  const std::chrono::microseconds up_threshold_;
  stream_t keepalives_stream_;
  storage_t storage_;
  const HTTPRoutesScope http_scope_;
};

using Karl = GenericKarl<DefaultClaireServiceStatus>;

}  // namespace current::karl
}  // namespace current

#endif  // KARL_KARL_H
