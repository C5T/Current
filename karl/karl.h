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
// 1) The Stream `Stream` of all keepalives received. Persisted on disk, not stored in memory.
//    Each "visualize production" request (be it JSON or SVG response) replays that stream over the desired
//    period of time. Most commonly it's the past five minutes.
//
// 2) The `Storage`, over a separate stream, to retain the information which may be required outside the
//    "visualized" time window. Includes Karl's launch history, and per-service codename -> build::BuildInfo.
//
// TODO(dkorolev) + TODO(mzhurovich): Stuff like nginx config for fresh start lives in the Storage part, right?
//                                    We'll need to have GenericKarl accept custom storage type then.
//
// The conventional wisdom is that Karl can start with both 1) and 2) missing. After one keepalive cycle,
// which is under half a minute, it would regain the state of the fleet, as long as all keepalives go to it.

// NOTE: The user must `#include` their local `current_build.h` header prior to Karl/Claire headers.

#ifndef KARL_KARL_H
#define KARL_KARL_H

#include "../port.h"

#include <type_traits>

// The `current_build.h` file from this local `Current/Karl` dir makes no sense for external users of Karl.
// Nonetheless, top-level `make test` and `make check` should pass out of the box.
#ifdef CURRENT_MAKE_CHECK_MODE
#include "current_build.h.mock"
#endif

#include "exceptions.h"
#include "schema_karl.h"
#include "schema_claire.h"
#include "locator.h"
#include "render.h"
#include "respond_with_schema.h"

#include "../storage/storage.h"
#include "../storage/persister/stream.h"

#include "../bricks/net/http/impl/server.h"
#include "../bricks/util/base64.h"

#include "../blocks/http/api.h"

#include "../utils/nginx/nginx.h"

#ifdef EXTRA_KARL_LOGGING
#include "../typesystem/schema/schema.h"
#endif

namespace current {
namespace karl {

CURRENT_STRUCT_T(KarlPersistedKeepalive) {
  CURRENT_FIELD(location, ClaireServiceKey);
  CURRENT_FIELD(keepalive, T);
};

template <class STORAGE>
class KarlNginxManager {
 protected:
  explicit KarlNginxManager(current::Borrowed<STORAGE> storage,
                            const KarlNginxParameters& nginx_parameters,
                            uint16_t karl_keepalives_port,
                            uint16_t karl_fleet_view_port)
      : storage_(std::move(storage)),
        has_nginx_config_file_(!nginx_parameters.config_file.empty()),
        nginx_parameters_(nginx_parameters),
        karl_keepalives_port_(karl_keepalives_port),
        karl_fleet_view_port_(karl_fleet_view_port),
        last_reflected_stream_head_(std::chrono::microseconds(-1)) {
    if (has_nginx_config_file_) {
      if (!nginx::NginxInvoker().IsNginxAvailable()) {
        CURRENT_THROW(NginxRequestedButNotAvailableException());
      }
      if (nginx_parameters_.port == 0u) {
        CURRENT_THROW(NginxParametersInvalidPortException());
      }
      nginx_manager_ = std::make_unique<nginx::NginxManager>(nginx_parameters.config_file);
    }
  }

  void UpdateNginxIfNeeded() {
    // To spawn Nginx `server` at startup even if the storage is empty.
    static bool first_run = true;
    if (has_nginx_config_file_) {
      const std::chrono::microseconds current_stream_head = storage_->LastAppliedTimestamp();
      if (first_run || current_stream_head != last_reflected_stream_head_) {
        nginx::config::ServerDirective server(nginx_parameters_.port);
        server.CreateProxyPassLocation("/", Printf("http://localhost:%d/", karl_fleet_view_port_));
        storage_->ReadOnlyTransaction([this, &server](ImmutableFields<STORAGE> fields) -> void {
          // Proxy status pages of the services via `{route_prefix}/{codename}`.
          for (const auto& claire : fields.claires) {
            if (claire.registered_state == ClaireRegisteredState::Active) {
              auto& location = server.CreateProxyPassLocation(nginx_parameters_.route_prefix + '/' + claire.codename,
                                                              claire.location.StatusPageURL());
              // Block all HTTP methods except `GET`.
              location.Add(nginx::config::BlockDirective(
                  "limit_except", "GET", {nginx::config::SimpleDirective("deny", "all")}));
            }
          }
        }).Go();
        nginx_manager_->UpdateConfig(std::move(server));
        last_reflected_stream_head_ = current_stream_head;
        first_run = false;
      }
    }
  }

  virtual ~KarlNginxManager() {}

 protected:
  Borrowed<STORAGE> storage_;
  const bool has_nginx_config_file_;
  const KarlNginxParameters nginx_parameters_;
  const uint16_t karl_keepalives_port_;
  const uint16_t karl_fleet_view_port_;
  std::unique_ptr<nginx::NginxManager> nginx_manager_;

 private:
  std::chrono::microseconds last_reflected_stream_head_;
};

struct UseOwnStorage {};

// Storage initializer base class.
// `STORAGE_TYPE` is either a real type of the storage if an external storage is used, or a `UseOwnStorage`,
// which makes Karl create its own `ServiceStorage` instance.
template <class STORAGE_TYPE>
struct KarlStorage {
  using storage_t = STORAGE_TYPE;
  Borrowed<storage_t> storage_;

 protected:
  explicit KarlStorage(Borrowed<storage_t> storage) : storage_(std::move(storage)) {}
};

template <>
struct KarlStorage<UseOwnStorage> {
  using storage_t = ServiceStorage<StreamStreamPersister>;
  using stream_t = typename storage_t::stream_t;
  Owned<storage_t> storage_;

 protected:
  explicit KarlStorage(const std::string& persistence_file)
      : storage_(storage_t::CreateMasterStorage(persistence_file)) {}
};

// Interface to implement for receiving callbacks/notifications from Karl.
template <typename RUNTIME_STATUS_VARIANT>
class IKarlNotifiable {
 public:
  using claire_status_t = ClaireServiceStatus<RUNTIME_STATUS_VARIANT>;

  virtual ~IKarlNotifiable() = default;

  // A new keepalive has been received from a service.
  virtual void OnKeepalive(std::chrono::microseconds now,
                           const ClaireServiceKey& location,
                           const std::string& codename,
                           const claire_status_t&) = 0;
  // A service has just gracefully deregistered itself.
  virtual void OnDeregistered(std::chrono::microseconds now,
                              const std::string& codename,
                              const ImmutableOptional<ClaireInfo>&) = 0;
  // A service has timed out (45 seconds by default), without gracefully deregistering itself.
  virtual void OnTimedOut(std::chrono::microseconds now,
                          const std::string& codename,
                          const ImmutableOptional<ClaireInfo>&) = 0;
};

// Dummy class with no-op functions. Used by Karl if no custom notifiable class has been provided.
template <typename RUNTIME_STATUS_VARIANT>
class DummyKarlNotifiable : public IKarlNotifiable<RUNTIME_STATUS_VARIANT> {
 public:
  using claire_status_t = ClaireServiceStatus<RUNTIME_STATUS_VARIANT>;
  void OnKeepalive(std::chrono::microseconds,
                   const ClaireServiceKey&,
                   const std::string&,
                   const claire_status_t&) override {}
  void OnDeregistered(std::chrono::microseconds, const std::string&, const ImmutableOptional<ClaireInfo>&) override {}
  void OnTimedOut(std::chrono::microseconds, const std::string&, const ImmutableOptional<ClaireInfo>&) override {}
};

template <class STORAGE_TYPE, typename... TS>
class GenericKarl final : private KarlStorage<STORAGE_TYPE>,
                          private KarlNginxManager<typename KarlStorage<STORAGE_TYPE>::storage_t>,
                          private DummyKarlNotifiable<Variant<TS...>>,
                          private DefaultFleetViewRenderer<Variant<TS...>> {
 public:
  using runtime_status_variant_t = Variant<TS...>;
  using claire_status_t = ClaireServiceStatus<runtime_status_variant_t>;
  using karl_status_t = GenericKarlStatus<runtime_status_variant_t>;
  using persisted_keepalive_t = KarlPersistedKeepalive<claire_status_t>;
  using stream_t = stream::Stream<persisted_keepalive_t, current::persistence::File>;
  using storage_t = typename KarlStorage<STORAGE_TYPE>::storage_t;
  using karl_notifiable_t = IKarlNotifiable<runtime_status_variant_t>;
  using fleet_view_renderer_t = IKarlFleetViewRenderer<runtime_status_variant_t>;

  template <class S = STORAGE_TYPE, class = std::enable_if_t<std::is_same_v<S, UseOwnStorage>>>
  explicit GenericKarl(const KarlParameters& parameters)
      : GenericKarl(parameters.storage_persistence_file, parameters, *this, *this, PrivateConstructorSelector()) {}
  GenericKarl(const KarlParameters& parameters, karl_notifiable_t& notifiable)
      : GenericKarl(parameters.storage_persistence_file, parameters, notifiable, *this, PrivateConstructorSelector()) {}
  GenericKarl(const KarlParameters& parameters, fleet_view_renderer_t& renderer)
      : GenericKarl(parameters.storage_persistence_file, parameters, *this, renderer, PrivateConstructorSelector()) {}
  GenericKarl(const KarlParameters& parameters, karl_notifiable_t& notifiable, fleet_view_renderer_t& renderer)
      : GenericKarl(
            parameters.storage_persistence_file, parameters, notifiable, renderer, PrivateConstructorSelector()) {}

  template <class S = STORAGE_TYPE, class = std::enable_if_t<!std::is_same_v<S, UseOwnStorage>>>
  GenericKarl(Borrowed<STORAGE_TYPE> storage, const KarlParameters& parameters)
      : GenericKarl(storage, parameters, *this, *this, PrivateConstructorSelector()) {}
  GenericKarl(Borrowed<STORAGE_TYPE> storage, const KarlParameters& parameters, karl_notifiable_t& notifiable)
      : GenericKarl(storage, parameters, notifiable, *this, PrivateConstructorSelector()) {}
  GenericKarl(Borrowed<STORAGE_TYPE> storage, const KarlParameters& parameters, fleet_view_renderer_t& renderer)
      : GenericKarl(storage, parameters, *this, renderer, PrivateConstructorSelector()) {}
  GenericKarl(Borrowed<STORAGE_TYPE> storage,
              const KarlParameters& parameters,
              karl_notifiable_t& notifiable,
              fleet_view_renderer_t& renderer)
      : GenericKarl(storage, parameters, notifiable, renderer, PrivateConstructorSelector()) {}

 private:
  using karl_storage_t = KarlStorage<STORAGE_TYPE>;
  using karl_nginx_manager_t = KarlNginxManager<typename karl_storage_t::storage_t>;
  // We need these `using`s because of template base classes :(
  using karl_storage_t::storage_;
  using karl_nginx_manager_t::UpdateNginxIfNeeded;
  using karl_nginx_manager_t::has_nginx_config_file_;
  using karl_nginx_manager_t::nginx_parameters_;

  struct PrivateConstructorSelector {};
  template <typename T>
  GenericKarl(T& storage_or_file,
              const KarlParameters& parameters,
              IKarlNotifiable<runtime_status_variant_t>& notifiable,
              IKarlFleetViewRenderer<runtime_status_variant_t>& renderer,
              PrivateConstructorSelector)
      : karl_storage_t(storage_or_file),
        karl_nginx_manager_t(
            storage_, parameters.nginx_parameters, parameters.keepalives_port, parameters.fleet_view_port),
        destructing_(false),
        parameters_(parameters),
        actual_public_url_(parameters_.public_url == kDefaultFleetViewURL
                               ? current::strings::Printf(kDefaultFleetViewURL, parameters_.nginx_parameters.port)
                               : parameters_.public_url),
        notifiable_ref_(notifiable),
        fleet_view_renderer_ref_(renderer),
        keepalives_stream_(stream_t::CreateStream(parameters_.stream_persistence_file)),
        state_update_thread_running_(false),
        state_update_thread_force_wakeup_(false),
        state_update_thread_([this]() {
          state_update_thread_running_ = true;
          StateUpdateThread();
        }),
        http_scope_(HTTP(parameters_.keepalives_port)
                        .Register(parameters_.keepalives_url,
                                  URLPathArgs::CountMask::None | URLPathArgs::CountMask::One,
                                  [this](Request r) { AcceptKeepaliveViaHTTP(std::move(r)); }) +
                    HTTP(parameters_.fleet_view_port)
                        .Register(parameters_.fleet_view_url,
                                  URLPathArgs::CountMask::None,
                                  [this](Request r) { ServeFleetStatus(std::move(r)); }) +
                    HTTP(parameters_.fleet_view_port)
                        .Register(parameters_.fleet_view_url + "status",
                                  URLPathArgs::CountMask::None,
                                  [this](Request r) { ServeKarlStatus(std::move(r)); }) +
                    HTTP(parameters_.fleet_view_port)
                        .Register(parameters_.fleet_view_url + "build",
                                  URLPathArgs::CountMask::One,
                                  [this](Request r) { ServeBuild(std::move(r)); }) +
                    HTTP(parameters_.fleet_view_port)
                        .Register(parameters_.fleet_view_url + "snapshot",
                                  URLPathArgs::CountMask::One,
                                  [this](Request r) { ServeSnapshot(std::move(r)); }) +
                    HTTP(parameters_.fleet_view_port)
                        .Register(parameters_.fleet_view_url + "favicon.png", http::CurrentFaviconHandler())) {
    while (!state_update_thread_running_) {
      // Starting Karl is a rare operation, and it may take a while on an overloaded CPU.
      // Thus, `std::this_thread::yield()` would be an overkill, we want this code to behave on a busy system.
      // A condition variable or a `WaitableAtomic` is a cleaner solution here; for now, just sleep.
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (parameters_.keepalives_port == parameters_.fleet_view_port) {
      std::cerr << "It's advised to start Karl with two different ports: "
                << "one to accept keepalives and one to serve status. "
                << "Please fix this and restart the binary. Thanks." << std::endl;
      std::exit(-1);
    }
    // Report this Karl as up and running.
    storage_->ReadWriteTransaction([this](MutableFields<storage_t> fields) {
      const auto& keepalives_data = keepalives_stream_->Data();
      KarlInfo self_info;
      if (!keepalives_data->Empty()) {
        self_info.persisted_keepalives_info = keepalives_data->LastPublishedIndexAndTimestamp();
      }
      fields.karl.Add(self_info);

      const auto now = current::time::Now();
      // Pre-populate services marked as `Active` due to abrupt shutdown into the cache.
      // This will help to eventually mark non-active services as `DisconnectedByTimeout`.
      for (const auto& claire : fields.claires) {
        if (claire.registered_state == ClaireRegisteredState::Active) {
          services_keepalive_time_cache_[claire.codename] = now;
        }
      }
    }).Wait();
  }

 public:
  ~GenericKarl() {
    destructing_ = true;
    storage_->ReadWriteTransaction([](MutableFields<storage_t> fields) {
      KarlInfo self_info;
      self_info.up = false;
      fields.karl.Add(self_info);
    }).Wait();
    if (state_update_thread_.joinable()) {
      {
        std::unique_lock<std::mutex> lock(services_keepalive_cache_mutex_);
        update_thread_condition_variable_.notify_one();
      }
      state_update_thread_.join();
    } else {
      // TODO(dkorolev), #FIXME_DIMA: This should not happen.
      std::cerr << "!state_update_thread_.joinable()\n";
      std::exit(-1);
    }
  }

  size_t ActiveServicesCount() const {
    std::lock_guard<std::mutex> lock(services_keepalive_cache_mutex_);
    return services_keepalive_time_cache_.size();
  }

  std::set<std::string> LocalIPs() const {
    std::lock_guard<std::mutex> lock(local_ips_mutex_);
    return local_ips_;
  }

  Borrowed<storage_t> BorrowStorage() const { return storage_; }

 private:
  void StateUpdateThread() {
    while (!destructing_) {
      const auto now = current::time::Now();
      std::unordered_set<std::string> timeouted_codenames;
      std::chrono::microseconds most_recent_keepalive_time(0);
      {
        std::lock_guard<std::mutex> lock(services_keepalive_cache_mutex_);
        for (auto it = services_keepalive_time_cache_.cbegin(); it != services_keepalive_time_cache_.cend();) {
          if ((now - it->second) > parameters_.service_timeout_interval) {
            timeouted_codenames.insert(it->first);
            services_keepalive_time_cache_.erase(it++);
          } else {
            most_recent_keepalive_time = std::max(it->second, most_recent_keepalive_time);
            ++it;
          }
        }
      }
      if (!timeouted_codenames.empty()) {
        auto& notifiable_ref = notifiable_ref_;
        storage_->ReadWriteTransaction([&timeouted_codenames, now, &notifiable_ref](MutableFields<storage_t> fields)
                                           -> void {
                                             for (const auto& codename : timeouted_codenames) {
                                               const auto& current_claire_info = fields.claires[codename];
                                               // OK to call from within a transaction.
                                               // The call is fast, and `storage_`'s transaction guarantees thread
                                               // safety. -- D.K.
                                               ClaireInfo claire;
                                               if (Exists(current_claire_info)) {
                                                 claire = Value(current_claire_info);
                                                 notifiable_ref.OnTimedOut(now, codename, current_claire_info);
                                               } else {
                                                 claire.codename = codename;
                                               }
                                               claire.registered_state = ClaireRegisteredState::DisconnectedByTimeout;
                                               fields.claires.Add(claire);
                                             }
                                           }).Wait();
      }
      UpdateNginxIfNeeded();
#ifdef CURRENT_MOCK_TIME
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
#else
      std::unique_lock<std::mutex> lock(services_keepalive_cache_mutex_);
      if (most_recent_keepalive_time.count() != 0) {
        const auto wait_interval =
            parameters_.service_timeout_interval - (current::time::Now() - most_recent_keepalive_time);
        if (wait_interval.count() > 0) {
          update_thread_condition_variable_.wait_for(
              lock,
              wait_interval + std::chrono::microseconds(1),
              [this]() { return destructing_ || state_update_thread_force_wakeup_; });
        }
      } else {
        update_thread_condition_variable_.wait(lock,
                                               [this]() { return destructing_ || state_update_thread_force_wakeup_; });
      }
      if (state_update_thread_force_wakeup_) {
        state_update_thread_force_wakeup_ = false;
      }
#endif
    }
  }

  void AcceptKeepaliveViaHTTP(Request r) {
    if (r.method != "GET" && r.method != "POST" && r.method != "DELETE") {
      r(current::net::DefaultMethodNotAllowedMessage(),
        HTTPResponseCode.MethodNotAllowed,
        net::http::Headers(),
        net::constants::kDefaultHTMLContentType);
      return;
    }

    const auto& qs = r.url.query;

    {
      std::lock_guard<std::mutex> lock(local_ips_mutex_);
      local_ips_.insert(r.connection.LocalIPAndPort().ip);
    }
    const std::string remote_ip = r.connection.RemoteIPAndPort().ip;

    if (r.method == "DELETE") {
      if (qs.has("codename")) {
        const std::string codename = qs["codename"];
        const auto now = current::time::Now();
        auto& notifiable_ref = notifiable_ref_;
        storage_->ReadWriteTransaction([codename, now, &notifiable_ref](MutableFields<storage_t> fields) -> Response {
          const auto& current_claire_info = fields.claires[codename];
          // OK to call from within a transaction.
          // The call is fast, and `storage_`'s transaction guarantees thread safety. -- D.K.
          notifiable_ref.OnDeregistered(now, codename, current_claire_info);
          ClaireInfo claire;
          if (Exists(current_claire_info)) {
            claire = Value(current_claire_info);
          } else {
            claire.codename = codename;
          }
          claire.registered_state = ClaireRegisteredState::Deregistered;
          fields.claires.Add(claire);
          return Response("OK\n");
        }, std::move(r)).Wait();  // NOTE(dkorolev): Could be `.Detach()`, but staying "safe" within Karl for now.
        {
          // Delete this `codename` from cache.
          std::lock_guard<std::mutex> lock(services_keepalive_cache_mutex_);
          services_keepalive_time_cache_.erase(codename);
          state_update_thread_force_wakeup_ = true;
          update_thread_condition_variable_.notify_one();
        }
      } else {
        // Respond with "200 OK" in any case.
        r("NOP\n");
      }
    } else {
      // Accepting keepalives as either POST or GET now, no harm in this. -- D.K.
      try {
        // If `&confirm` is set, along with `codename` and `port`, Karl calls the service back
        // via the URL from the inbound request and the port the service has provided,
        // to confirm two-way communication.
        const std::string json = [&]() -> std::string {
          if (qs.has("confirm") && qs.has("port")) {
            const std::string url = "http://" + remote_ip + ':' + qs["port"] + "/.current";
            // Send a GET request, with a random component in the URL to prevent caching.
            return HTTP(GET(url + "?all&rnd" + current::ToString(current::random::CSRandomUInt(static_cast<uint32_t>(1e9), static_cast<uint32_t>(2e9))))).body;
          } else {
            return r.body;
          }
        }();

        const auto parsed_status = ParseJSON<ClaireStatus>(json);
        if ((!qs.has("codename") || parsed_status.codename == qs["codename"]) &&
            (!qs.has("port") || parsed_status.local_port == current::FromString<uint16_t>(qs["port"]))) {
          ClaireServiceKey location;
          location.ip = remote_ip;
          location.port = parsed_status.local_port;
          location.prefix = "/";  // TODO(dkorolev) + TODO(mzhurovich): Add support for `qs["prefix"]`.

          // If the received status can be parsed in detail, including the "runtime" variant, persist it.
          // If no, no big deal, keep the top-level one regardless.
          const auto detailed_parsed_status = [&]() -> claire_status_t {
            Optional<claire_status_t> parsed_opt = TryParseJSON<claire_status_t>(json);
            if (!Exists(parsed_opt)) {  // Can't parse in Current format. Trying `Minimalistic`.
              parsed_opt = TryParseJSON<claire_status_t, JSONFormat::Minimalistic>(json);
            }
            if (Exists(parsed_opt)) {
              return Value(parsed_opt);
            } else {

#ifdef EXTRA_KARL_LOGGING
              std::cerr << "Could not parse: " << json << '\n';
              reflection::StructSchema struct_schema;
              struct_schema.AddType<claire_status_t>();
              std::cerr << "As:\n" << struct_schema.GetSchemaInfo().Describe<reflection::Language::Current>() << '\n';
#endif

              claire_status_t status;
              // Initialize `ClaireStatus` from `ClaireServiceStatus`, keep the `Variant<...> runtime` empty.
              static_cast<ClaireStatus&>(status) = parsed_status;
              return status;
            }
          }();

          const auto now = current::time::Now();

          {
            persisted_keepalive_t record;
            record.location = location;
            record.keepalive = detailed_parsed_status;
            {
              std::lock_guard<std::mutex> lock(latest_keepalive_index_mutex_);
              latest_keepalive_index_plus_one_[parsed_status.codename] =
                  keepalives_stream_->Publisher()->Publish(std::move(record)).index;
            }
          }

          Optional<std::chrono::microseconds> optional_behind_this_by;
          if (Exists(parsed_status.last_successful_keepalive_ping_us)) {
            optional_behind_this_by =
                now - parsed_status.now - Value(parsed_status.last_successful_keepalive_ping_us) / 2;
          }

          auto& notifiable_ref = notifiable_ref_;
          storage_
              ->ReadWriteTransaction(
                    [now, location, &parsed_status, &detailed_parsed_status, optional_behind_this_by, &notifiable_ref](
                        MutableFields<storage_t> fields) -> Response {
                      // OK to call from within a transaction.
                      // The call is fast, and `storage_`'s transaction guarantees thread safety. -- D.K.
                      notifiable_ref.OnKeepalive(now, location, parsed_status.codename, detailed_parsed_status);

                      const auto& service = parsed_status.service;
                      const auto& codename = parsed_status.codename;
                      const auto& optional_build = parsed_status.build;
                      const auto& optional_instance = parsed_status.cloud_instance_name;
                      const auto& optional_av_group = parsed_status.cloud_availability_group;

                      // Update per-server information in the `DB`.
                      ServerInfo server;
                      server.ip = location.ip;
                      bool need_to_update_server_info = false;
                      const ImmutableOptional<ServerInfo> current_server_info = fields.servers[location.ip];
                      if (Exists(current_server_info)) {
                        server = Value(current_server_info);
                      }
                      // Check the instance name.
                      if (Exists(optional_instance)) {
                        if (!Exists(server.cloud_instance_name) ||
                            Value(server.cloud_instance_name) != Value(optional_instance)) {
                          server.cloud_instance_name = Value(optional_instance);
                          need_to_update_server_info = true;
                        }
                      }
                      // Check the availability group.
                      if (Exists(optional_av_group)) {
                        if (!Exists(server.cloud_availability_group) ||
                            Value(server.cloud_availability_group) != Value(optional_av_group)) {
                          server.cloud_availability_group = Value(optional_av_group);
                          need_to_update_server_info = true;
                        }
                      }
                      // Check the time skew.
                      if (Exists(optional_behind_this_by)) {
                        const std::chrono::microseconds behind_this_by = Value(optional_behind_this_by);
                        const auto time_skew_difference = server.behind_this_by - behind_this_by;
                        if (static_cast<uint64_t>(std::abs(time_skew_difference.count())) >=
                            kUpdateServerInfoThresholdByTimeSkewDifference) {
                          server.behind_this_by = behind_this_by;
                          need_to_update_server_info = true;
                        }
                      }
                      if (need_to_update_server_info) {
                        fields.servers.Add(server);
                      }

                      // Update the `DB` if the build information was not stored there yet.
                      const ImmutableOptional<ClaireBuildInfo> current_claire_build_info = fields.builds[codename];
                      if (Exists(optional_build) && (!Exists(current_claire_build_info) ||
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
                            } else if (Value(current_claire_info).registered_state != ClaireRegisteredState::Active) {
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
                        claire.service = service;
                        claire.location = location;
                        claire.reported_timestamp = now;
                        claire.url_status_page_direct = location.StatusPageURL();
                        claire.registered_state = ClaireRegisteredState::Active;

                        fields.claires.Add(claire);
                      }
                      return Response("OK\n");
                    },
                    std::move(r))
              .Wait();
          {
            std::lock_guard<std::mutex> lock(services_keepalive_cache_mutex_);
            auto& placeholder = services_keepalive_time_cache_[parsed_status.codename];
            if (placeholder.count() == 0) {
              placeholder = now;
              // Wake up state update thread only if the new codename has appeared in the cache.
              state_update_thread_force_wakeup_ = true;
              update_thread_condition_variable_.notify_one();
            } else {
              placeholder = now;
            }
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
    }
  }

  void ServeFleetStatus(Request r) {
    const auto& qs = r.url.query;
    if (qs.has("schema")) {
      using uber_t = Variant<karl_status_t, claire_status_t, persisted_keepalive_t, KarlParameters>;
      const auto& schema = qs["schema"];
      using L = current::reflection::Language;
      if (schema == "md") {
        RespondWithSchema<uber_t, L::Markdown>(std::move(r));
      } else if (schema == "fs") {
        RespondWithSchema<uber_t, L::FSharp>(std::move(r));
      } else {
        RespondWithSchema<uber_t, L::JSON>(std::move(r));
      }
    } else {
      BuildStatusAndRespondWithIt(std::move(r));
    }
  }

  void ServeKarlStatus(Request r) {
    if (r.url.query.has("parameters") || r.url.query.has("params") || r.url.query.has("p") || r.url.query.has("full") ||
        r.url.query.has("f")) {
      r(KarlUpStatus(parameters_));
    } else {
      // TODO(dkorolev) + TODO(mzhurovich): JSON headers magic.
      r(JSON<JSONFormat::Minimalistic>(KarlUpStatus()) + '\n',
        HTTPResponseCode.OK,
        current::net::http::Headers(),
        current::net::constants::kDefaultJSONContentType);
    }
  }

  void ServeBuild(Request r) {
    const auto codename = r.url_path_args[0];
    storage_->ReadOnlyTransaction([codename](ImmutableFields<storage_t> fields) -> Response {
      const auto result = fields.builds[codename];
      if (Exists(result)) {
        return Value(result);
      } else {
        return Response(current_service_state::Error("Codename '" + codename + "' not found."),
                        HTTPResponseCode.NotFound);
      }
    }, std::move(r)).Wait();  // NOTE(dkorolev): Could be `.Detach()`, but staying "safe" within Karl for now.
  }

  void ServeSnapshot(Request r) {
    const auto codename = r.url_path_args[0];

    uint64_t& index_placeholder = [&]() {
      std::lock_guard<std::mutex> lock(latest_keepalive_index_mutex_);
      return std::ref(latest_keepalive_index_plus_one_[codename]);
    }();

    const auto& keepalives_data = keepalives_stream_->Data();

    uint64_t index = index_placeholder;
    if (!index) {
      // If no latest keepalive index in cache, go through the whole log.
      for (const auto& e : keepalives_data->Iterate()) {
        if (e.entry.keepalive.codename == codename) {
          index = e.idx_ts.index + 1;
        }
      }
      if (index) {
        std::lock_guard<std::mutex> lock(latest_keepalive_index_mutex_);
        index_placeholder = std::max(index_placeholder, index);
      }
    }

    if (index) {
      const auto e = *(keepalives_data->Iterate(index - 1).begin());
      if (!r.url.query.has("nobuild")) {
        r(JSON<JSONFormat::Minimalistic>(
              SnapshotOfKeepalive<runtime_status_variant_t>(e.idx_ts.us - current::time::Now(), e.entry.keepalive)),
          HTTPResponseCode.OK,
          current::net::http::Headers(),
          current::net::constants::kDefaultJSONContentType);
      } else {
        auto tmp = e.entry.keepalive;
        tmp.build = nullptr;
        r(JSON<JSONFormat::Minimalistic>(
              SnapshotOfKeepalive<runtime_status_variant_t>(e.idx_ts.us - current::time::Now(), tmp)),
          HTTPResponseCode.OK,
          current::net::http::Headers(),
          current::net::constants::kDefaultJSONContentType);
      }
    } else {
      r(current_service_state::Error("No keepalives from '" + codename + "' have been received."),
        HTTPResponseCode.NotFound);
    }
  }

  void BuildStatusAndRespondWithIt(Request r) {
    // For a GET response, compile the status page and return it.
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
        return now - std::chrono::microseconds(
                         static_cast<int64_t>(current::FromString<double>(r.url.query["d"]) * 1e6 * 60 * 60 * 24));
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

    CURRENT_ASSERT(to >= from);
    const auto& keepalives_data(keepalives_stream_->Data());
    for (const auto& e : keepalives_data->Iterate(from, to)) {
      const claire_status_t& keepalive = e.entry.keepalive;

      codenames_to_resolve.insert(keepalive.codename);
      service_key_into_codename[e.entry.location] = keepalive.codename;

      codenames_per_service[keepalive.service].insert(keepalive.codename);
      // DIMA: More per-codename reporting fields go here; tailored to specific type, `.Call(populator)`, etc.
      ProtoReport report;
      const std::string last_keepalive =
          current::strings::TimeIntervalAsHumanReadableString(now - e.idx_ts.us) + " ago";
      if ((now - e.idx_ts.us) < parameters_.service_timeout_interval) {
        // Service is up.
        const auto projected_uptime_us =
            (keepalive.now - keepalive.start_time_epoch_microseconds) + (now - e.idx_ts.us);
        report.currently =
            current_service_state::up(keepalive.start_time_epoch_microseconds,
                                      last_keepalive,
                                      e.idx_ts.us,
                                      current::strings::TimeIntervalAsHumanReadableString(projected_uptime_us));
      } else {
        // Service is down.
        // TODO(dkorolev): Graceful shutdown case for `done`.
        report.currently = current_service_state::down(
            keepalive.start_time_epoch_microseconds, last_keepalive, e.idx_ts.us, keepalive.uptime);
      }
      report.dependencies = keepalive.dependencies;
      report.runtime = keepalive.runtime;
      report_for_codename[keepalive.codename] = report;
    }

    // To list only the services that are currently in `Active` state.
    const bool active_only = r.url.query.has("active_only");

    const auto response_format = [&r]() -> FleetViewResponseFormat {
      if (r.url.query.has("full")) {
        return FleetViewResponseFormat::JSONFull;
      }
      if (r.url.query.has("json")) {
        return FleetViewResponseFormat::JSONMinimalistic;
      }
      if (r.url.query.has("html")) {
        return FleetViewResponseFormat::HTMLFormat;
      }
      const char* kAcceptHeader = "Accept";
      if (r.headers.Has(kAcceptHeader)) {
        for (const auto& h : strings::Split(r.headers[kAcceptHeader].value, ',')) {
          if (strings::Split(h, ';').front() == "text/html") {  // Allow "text/html; charset=...", etc.
            return FleetViewResponseFormat::HTMLFormat;
          }
        }
      }
      return FleetViewResponseFormat::JSONMinimalistic;
    }();

    const std::string public_url = actual_public_url_;
    storage_->ReadOnlyTransaction(
                  [this,
                   now,
                   from,
                   to,
                   active_only,
                   response_format,
                   public_url,
                   codenames_to_resolve,
                   report_for_codename,
                   codenames_per_service,
                   service_key_into_codename](ImmutableFields<storage_t> fields) -> Response {
                    std::unordered_map<std::string, ClaireServiceKey> resolved_codenames;
                    karl_status_t result;
                    result.now = now;
                    result.from = from;
                    result.to = to;
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

                        {
                          const auto optional_build = fields.builds[codename];
                          if (Exists(optional_build)) {
                            const auto& info = Value(optional_build).build;
                            blob.build_time = info.build_time;
                            blob.build_time_epoch_microseconds = info.build_time_epoch_microseconds;
                            blob.git_commit = info.git_commit_hash;
                            blob.git_branch = info.git_branch;
                            blob.git_dirty = Exists(info.git_dirty_files) && !Value(info.git_dirty_files).empty();
                          }
                        }

                        if (has_nginx_config_file_) {
                          blob.url_status_page_proxied =
                              actual_public_url_ + nginx_parameters_.route_prefix + '/' + codename;
                        }
                        blob.url_status_page_direct = blob.location.StatusPageURL();
                        blob.location = resolved_codenames[codename];
                        blob.runtime = rhs.runtime;
                        result.machines[blob.location.ip].services[codename] = std::move(blob);
                      }
                    }
                    // Update per-server information.
                    for (auto& iterating_over_reported_servers : result.machines) {
                      const std::string& ip = iterating_over_reported_servers.first;
                      auto& server = iterating_over_reported_servers.second;
                      const auto& optional_persisted_server_info = fields.servers[ip];
                      if (Exists(optional_persisted_server_info)) {
                        const auto& persisted_server_info = Value(optional_persisted_server_info);
                        server.cloud_instance_name = persisted_server_info.cloud_instance_name;
                        server.cloud_availability_group = persisted_server_info.cloud_availability_group;
                        const int64_t behind_this_by_us = persisted_server_info.behind_this_by.count();
                        if (std::abs(behind_this_by_us) < 100000) {
                          server.time_skew = "NTP OK";
                        } else if (behind_this_by_us > 0) {
                          server.time_skew = current::strings::Printf("behind by %.1lfs", 1e-6 * behind_this_by_us);
                        } else {
                          server.time_skew = current::strings::Printf("ahead by %.1lfs", 1e-6 * behind_this_by_us);
                        }
                      }
                    }
                    result.generation_time = current::time::Now() - now;
                    return fleet_view_renderer_ref_.RenderResponse(response_format, parameters_, std::move(result));
                  },
                  std::move(r))
        .Wait();  // NOTE(dkorolev): Could be `.Detach()`, but staying "safe" within Karl for now.
  }

  std::atomic_bool destructing_;
  const KarlParameters parameters_;
  const std::string actual_public_url_;  // Derived from `parameters_` upon construction.
  IKarlNotifiable<runtime_status_variant_t>& notifiable_ref_;
  IKarlFleetViewRenderer<runtime_status_variant_t>& fleet_view_renderer_ref_;
  std::unordered_map<std::string, std::chrono::microseconds> services_keepalive_time_cache_;
  mutable std::mutex services_keepalive_cache_mutex_;
  std::set<std::string> local_ips_;  // The list of local IPs used ti receive keepalives.
  mutable std::mutex local_ips_mutex_;

  // codename -> stream index of the most recent keepalive from this codename.
  std::mutex latest_keepalive_index_mutex_;
  // Plus one to have `0` == "no keepalives", and avoid the corner case of record at index 0 being the one.
  std::unordered_map<std::string, uint64_t> latest_keepalive_index_plus_one_;

  current::Owned<stream_t> keepalives_stream_;
  std::atomic_bool state_update_thread_running_;
  std::atomic_bool state_update_thread_force_wakeup_;
  std::condition_variable update_thread_condition_variable_;
  std::thread state_update_thread_;
  const HTTPRoutesScope http_scope_;
};

using Karl = GenericKarl<UseOwnStorage, default_user_status::status>;

}  // namespace current::karl
}  // namespace current

#endif  // KARL_KARL_H
