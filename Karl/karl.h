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

CURRENT_STRUCT(ClaireBuildInfo) {
  CURRENT_FIELD(codename, std::string);
  CURRENT_USE_FIELD_AS_KEY(codename);

  CURRENT_FIELD(service, std::string);
  CURRENT_FIELD(location, std::string);  // TODO(dkorolev): A record of separate IP, port, URL prefix.
  CURRENT_FIELD(build, current::build::Info);
  CURRENT_FIELD(reported_timestamp, std::chrono::microseconds);
};

CURRENT_STORAGE_FIELD_ENTRY(OrderedDictionary, KarlInfo, KarlInfoDictionary);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedDictionary, ClaireBuildInfo, ClaireBuildInfoDictionary);

CURRENT_STORAGE(ServiceStorage) {
  CURRENT_STORAGE_FIELD(karl, KarlInfoDictionary);
  CURRENT_STORAGE_FIELD(claires, ClaireBuildInfoDictionary);
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Karl's HTTP response(s) schema.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CURRENT_STRUCT(KarlStatus) {
  CURRENT_FIELD(keepalives_per_codename, (std::map<std::string, std::string>));
  CURRENT_FIELD(uptime_per_codename, (std::map<std::string, std::string>));
  CURRENT_FIELD(codenames_per_service, (std::map<std::string, std::vector<std::string>>));
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Karl's implementation.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename... TS>
class GenericKarl final {
 public:
  using claire_status_t = ClaireServiceStatus<Variant<TS...>>;
  using stream_t = sherlock::Stream<claire_status_t, current::persistence::File>;
  using storage_t = ServiceStorage<SherlockStreamPersister>;

  explicit GenericKarl(uint16_t port,
                       const std::string& stream_persistence_file,
                       const std::string& storage_persistence_file,
                       const std::string& url = "/")
      : keepalives_stream_(stream_persistence_file),
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
    const auto& qs = r.url.query;
    // If `&confirm` is set, along with `codename` and `port`, Karl calls the service back
    // via the URL from the inbound request and the port the service has provided,
    // to confirm two-way communication.
    if (r.method == "POST" && qs.has("codename")) {
      try {
        const std::string location =
            "http://" + r.connection.RemoteIPAndPort().ip + ':' + qs["port"] + "/.current";
        const std::string json = [&]() -> std::string {
          if (qs.has("port") && qs.has("confirm")) {
            // Send a GET request, with a random component in the URL to prevent caching.
            return HTTP(GET(location + "?all&rnd" + current::ToString(current::random::CSRandomUInt(1e9, 2e9))))
                .body;
          } else {
            return r.body;
          }
        }();

        const auto body = ParseJSON<ClaireStatus>(json);

        if (body.codename == qs["codename"] && body.local_port == current::FromString<uint16_t>(qs["port"])) {
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
          keepalives_stream_.Publish(status);

          if (Exists(body.build)) {
            const auto build = Value(body.build);

            const std::string service = body.service;
            const std::string codename = body.codename;

            const auto now = current::time::Now();
            storage_.ReadWriteTransaction(
                         [now, codename, service, location, build](MutableFields<storage_t> fields)
                             -> Response {
                               const auto current = fields.claires[codename];
                               if (!(Exists(current) && Value(current).build == build)) {
                                 ClaireBuildInfo record;
                                 record.codename = codename;
                                 record.service = service;
                                 record.location = location;
                                 record.build = build;
                                 record.reported_timestamp = now;
                                 fields.claires.Add(record);
                               }
                               return Response("OK\n");
                             },
                         std::move(r)).Detach();
          } else {
            r("OK\n");
          }
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
      r(PrepareKarlStatus(from, to));
    }
  }

  KarlStatus PrepareKarlStatus(const std::chrono::microseconds from, const std::chrono::microseconds to) {
    const auto now = current::time::Now();
    KarlStatus result;
    std::map<std::string, std::set<std::string>> codenames_per_service;
    for (const auto& e : keepalives_stream_.InternalExposePersister().Iterate()) {
      if (e.idx_ts.us >= from && e.idx_ts.us < to) {
        const auto& keepalive = e.entry;
        result.keepalives_per_codename[keepalive.codename] = current::strings::TimeIntervalAsHumanReadableString(
                now - e.idx_ts.us) + " ago";
        result.uptime_per_codename[keepalive.codename] = keepalive.uptime;
        codenames_per_service[keepalive.service].insert(keepalive.codename);
      }
    }
    for (const auto& c : codenames_per_service) {
      result.codenames_per_service[c.first].assign(c.second.begin(), c.second.end());
    }
    return result;
  }

  stream_t keepalives_stream_;
  storage_t storage_;
  const HTTPRoutesScope http_scope_;
};

using Karl = GenericKarl<DefaultClaireServiceStatus>;

}  // namespace current::karl
}  // namespace current

#endif  // KARL_KARL_H
