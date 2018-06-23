/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_STORAGE_REST_H
#define CURRENT_STORAGE_REST_H

#include "../port.h"

#include <vector>
#include <map>

#include "api_types.h"

#include "rest/types.h"
#include "rest/plain.h"

#include "../typesystem/schema/schema.h"
#include "../blocks/http/api.h"
#include "../bricks/template/call_if.h"

namespace current {
namespace storage {
namespace rest {

// The means to exclude certain fields from being exposed via REST.
template <typename STORAGE_IMPL, typename PARTICULAR_FIELD>
struct FieldExposedViaREST {
  constexpr static bool exposed = true;
};

#define CURRENT_STORAGE_FIELD_EXCLUDE_FROM_REST(field) \
  namespace current {                                  \
  namespace storage {                                  \
  namespace rest {                                     \
  template <typename STORAGE>                          \
  struct FieldExposedViaREST<STORAGE, field> {         \
    constexpr static bool exposed = false;             \
  };                                                   \
  }                                                    \
  }                                                    \
  }

struct RESTfulRoute {
  std::string resource_prefix;                // "data" or "schema".
  std::string resource_suffix;                // "" or ".h", for per-language schema formats.
  URLPathArgs::CountMask resource_args_mask;  // `None|One` for  "data", `None` for "schema".
  std::function<void(Request)> handler;
  RESTfulRoute() = default;
  RESTfulRoute(const std::string& resource_prefix,
               const std::string& resource_suffix,
               URLPathArgs::CountMask mask,
               std::function<void(Request)> handler)
      : resource_prefix(resource_prefix),
        resource_suffix(resource_suffix),
        resource_args_mask(mask),
        handler(handler) {}
};

namespace impl {

// RESTful routes per fields. Multimap to allow `/{data|schema|map|etc.}/$FIELD` per `$FIELD`.
using storage_handlers_map_t = std::multimap<std::string, RESTfulRoute>;

// The inner `std::pair<>` of the above multimap.
using storage_handlers_map_entry_t = typename storage_handlers_map_t::value_type;

using registerer_t = std::function<void(const storage_handlers_map_entry_t&)>;

template <class REST_IMPL, int INDEX, typename STORAGE>
struct PerFieldRESTfulHandlerGenerator {
  using storage_t = STORAGE;
  using immutable_fields_t = ImmutableFields<STORAGE>;
  using mutable_fields_t = MutableFields<STORAGE>;

  // Field accessor type for this `INDEX`.
  // The type of `data.x` from within a `Transaction()`, where `x` is the field corresponding to index `INDEX`.
  using specific_field_extractor_t = decltype(std::declval<STORAGE>()(::current::storage::FieldTypeExtractor<INDEX>()));
  using specific_field_t = typename specific_field_extractor_t::particular_field_t;

  using specific_entry_type_extractor_t =
      decltype(std::declval<STORAGE>()(::current::storage::FieldEntryTypeExtractor<INDEX>()));
  using specific_entry_type_t = typename specific_entry_type_extractor_t::particular_field_t;

  template <class HTTP_VERB, typename OPERATION, typename FIELD, typename ENTRY, typename KEY>
  using DataHandlerImpl = typename REST_IMPL::template RESTfulDataHandler<HTTP_VERB, OPERATION, FIELD, ENTRY, KEY>;

  template <typename ENTRY>
  using SchemaHandlerImpl = typename REST_IMPL::template RESTfulSchemaHandler<STORAGE, ENTRY>;

  using field_rest_endpoints_schema_t = typename specific_field_t::semantics_t::rest_endpoints_schema_t;

  const registerer_t registerer;
  STORAGE& storage;
  const std::string restful_url_prefix;

  PerFieldRESTfulHandlerGenerator(registerer_t registerer, STORAGE& storage, const std::string& restful_url_prefix)
      : registerer(registerer), storage(storage), restful_url_prefix(restful_url_prefix) {}

  template <typename FIELD_TYPE, typename ENTRY_TYPE_WRAPPER>
  void operator()(const char* input_field_name, FIELD_TYPE, ENTRY_TYPE_WRAPPER) {
    auto& storage = this->storage;  // For lambdas.
    const std::string restful_url_prefix = this->restful_url_prefix;
    const std::string field_name = input_field_name;

    using entry_t = typename ENTRY_TYPE_WRAPPER::entry_t;
    using key_t = typename ENTRY_TYPE_WRAPPER::key_t;
    using top_level_operation_t = semantics::rest::operation::top_level_operation_for_field_t<specific_field_t>;

    using GETHandler = DataHandlerImpl<GET, top_level_operation_t, specific_field_t, entry_t, key_t>;
    using POSTHandler = DataHandlerImpl<POST, top_level_operation_t, specific_field_t, entry_t, key_t>;
    using PUTHandler = DataHandlerImpl<PUT, top_level_operation_t, specific_field_t, entry_t, key_t>;
    using PATCHHandler = DataHandlerImpl<PATCH, top_level_operation_t, specific_field_t, entry_t, key_t>;
    using DELETEHandler = DataHandlerImpl<DELETE, top_level_operation_t, specific_field_t, entry_t, key_t>;

    const auto generic_data_handler = [&storage, restful_url_prefix, field_name](Request request) {
      // TODO(dkorolev): Pass `BorrowedWithCallback<Storage>` into the request handler.
      auto generic_input = RESTfulGenericInput<STORAGE>(storage, restful_url_prefix);
      std::lock_guard<std::mutex> lock(storage.UnderlyingStream()->Impl()->publishing_mutex);
      const bool is_master = storage.template IsMasterStorage<current::locks::MutexLockStatus::AlreadyLocked>();
      if (request.method == "GET") {
        GETHandler handler;
        Optional<FieldExportParams> requested_export_params;
        if (request.url.query.has(kRESTfulExportURLQueryParameter)) {
          FieldExportParams params;
          if (request.url.query[kRESTfulExportURLQueryParameter] == "detailed") {
            params.format = FieldExportFormat::Detailed;
          }
          params.nshards = FromString<uint32_t>(request.url.query.get(kRESTfulExportNShardsURLQueryParameter, "0"));
          params.shard = FromString<uint32_t>(request.url.query.get(kRESTfulExportShardURLQueryParameter, "0"));
          requested_export_params = std::move(params);
        }
        handler.Enter(
            std::move(request),
            // Capture by reference since this lambda is run synchronously.
            [&storage, &handler, &generic_input, &field_name, requested_export_params](
                Request request,
                const Optional<typename field_type_dependent_t<specific_field_t>::url_key_t>& url_key) {
              const specific_field_t& field = generic_input.storage(::current::storage::ImmutableFieldByIndex<INDEX>());
              generic_input.storage
                  .template ReadOnlyTransaction<current::locks::MutexLockStatus::AlreadyLocked>(
                       // Capture local variables by value for safe async transactions.
                       [&storage, handler, generic_input, &field, url_key, field_name, requested_export_params](
                           immutable_fields_t fields) -> Response {
                         using GETInput = RESTfulGETInput<STORAGE, specific_field_t>;
                         const GETInput input(
                             std::move(generic_input),
                             fields,
                             field,
                             field_name,
                             url_key,
                             storage.template IsMasterStorage<current::locks::MutexLockStatus::AlreadyLocked>(),
                             requested_export_params);
                         return handler.Run(input);
                       },
                       std::move(request))
                  .Detach();
            });
      } else if (request.method == "POST" && is_master) {
        POSTHandler handler;
        handler.Enter(
            std::move(request),
            // Capture by reference since this lambda is run synchronously.
            [&handler, &generic_input, &field_name](Request request) {
              try {
                const bool overwrite = request.url.query.has("overwrite");
                auto mutable_entry = ParseJSON<entry_t>(request.body);
                specific_field_t& field = generic_input.storage(::current::storage::MutableFieldByIndex<INDEX>());
                generic_input.storage.template ReadWriteTransaction<current::locks::MutexLockStatus::AlreadyLocked>(
                                          // Capture local variables by value for safe async transactions.
                                          [handler, generic_input, &field, mutable_entry, field_name, overwrite](
                                              mutable_fields_t fields) mutable -> Response {
                                            using POSTInput = RESTfulPOSTInput<STORAGE, specific_field_t, entry_t>;
                                            const POSTInput input(std::move(generic_input),
                                                                  fields,
                                                                  field,
                                                                  field_name,
                                                                  mutable_entry,
                                                                  overwrite);
                                            return handler.Run(input);
                                          },
                                          std::move(request)).Detach();
              } catch (const TypeSystemParseJSONException& e) {
                request(handler.ErrorBadJSON(e.OriginalDescription()));
              }
            });
      } else if (request.method == "PUT" && is_master) {
        PUTHandler handler;
        handler.Enter(
            std::move(request),
            // Capture by reference since this lambda is run synchronously.
            [&handler, &generic_input, &field_name](
                Request request, const typename field_type_dependent_t<specific_field_t>::url_key_t& input_url_key) {
              try {
                const auto url_key =
                    field_type_dependent_t<specific_field_t>::template ParseURLKey<key_t>(input_url_key);
                const auto entry = ParseJSON<entry_t>(request.body);
                const auto entry_key = field_type_dependent_t<specific_field_t>::ExtractOrComposeKey(entry);
                specific_field_t& field = generic_input.storage(::current::storage::MutableFieldByIndex<INDEX>());
                generic_input.storage.template ReadWriteTransaction<current::locks::MutexLockStatus::AlreadyLocked>(
                                          // Capture local variables by value for safe async transactions.
                                          [handler, generic_input, &field, url_key, entry, entry_key, field_name](
                                              mutable_fields_t fields) -> Response {
                                            using PUTInput = RESTfulPUTInput<STORAGE, specific_field_t, entry_t, key_t>;
                                            const PUTInput input(std::move(generic_input),
                                                                 fields,
                                                                 field,
                                                                 field_name,
                                                                 url_key,
                                                                 entry,
                                                                 entry_key);
                                            return handler.Run(input);
                                          },
                                          std::move(request)).Detach();
              } catch (const TypeSystemParseJSONException& e) {          // LCOV_EXCL_LINE
                request(handler.ErrorBadJSON(e.OriginalDescription()));  // LCOV_EXCL_LINE
              }
            });
      } else if (request.method == "PATCH" && is_master) {
        PATCHHandler handler;
        handler.Enter(
            std::move(request),
            // Capture by reference since this lambda is run synchronously.
            [&handler, &generic_input, &field_name](
                Request request, const typename field_type_dependent_t<specific_field_t>::url_key_t& input_url_key) {
              const auto url_key = field_type_dependent_t<specific_field_t>::template ParseURLKey<key_t>(input_url_key);
              const std::string patch_body = request.body;
              specific_field_t& field = generic_input.storage(::current::storage::MutableFieldByIndex<INDEX>());
              generic_input.storage.template ReadWriteTransaction<current::locks::MutexLockStatus::AlreadyLocked>(
                                        // Capture local variables by value for safe async transactions.
                                        [handler, generic_input, &field, url_key, field_name, patch_body](
                                            mutable_fields_t fields) -> Response {
                                          using PATCHInput =
                                              RESTfulPATCHInput<STORAGE, specific_field_t, entry_t, key_t>;
                                          const PATCHInput input(
                                              std::move(generic_input), fields, field, field_name, url_key, patch_body);
                                          return handler.Run(input);
                                        },
                                        std::move(request)).Detach();
            });
      } else if (request.method == "DELETE" && is_master) {
        DELETEHandler handler;
        handler.Enter(
            std::move(request),
            // Capture by reference since this lambda is run synchronously.
            [&handler, &generic_input, &field_name](
                Request request, const typename field_type_dependent_t<specific_field_t>::url_key_t& input_url_key) {
              const auto url_key = field_type_dependent_t<specific_field_t>::template ParseURLKey<key_t>(input_url_key);
              specific_field_t& field = generic_input.storage(::current::storage::MutableFieldByIndex<INDEX>());
              generic_input.storage.template ReadWriteTransaction<current::locks::MutexLockStatus::AlreadyLocked>(
                                        // Capture local variables by value for safe async transactions.
                                        [handler, generic_input, &field, url_key, field_name](mutable_fields_t fields)
                                            -> Response {
                                              using DELETEInput = RESTfulDELETEInput<STORAGE, specific_field_t, key_t>;
                                              const DELETEInput input(
                                                  std::move(generic_input), fields, field, field_name, url_key);
                                              return handler.Run(input);
                                            },
                                        std::move(request)).Detach();
            });
      } else {
        const std::string error_message =
            is_master ? "Supported methods: GET, PUT, PATCH, POST, DELETE." : "Supported methods: GET.";
        request(REST_IMPL::ErrorMethodNotAllowed(request.method, error_message));  // LCOV_EXCL_LINE
      }
    };

    registerer(storage_handlers_map_entry_t(
        field_name, RESTfulRoute(kRESTfulDataURLComponent, "", URLPathArgs::CountMask::Any, generic_data_handler)));

    RegisterAdditionalFieldDataHandlers<FIELD_TYPE, ENTRY_TYPE_WRAPPER>(
        field_name, field_rest_endpoints_schema_t(), generic_data_handler);

    // Schema handlers.

    SchemaHandlerImpl<entry_t>().RegisterRoutes(
        storage,
        [&](const std::string& route_suffix, const std::function<void(Request)> handler) {
          registerer(storage_handlers_map_entry_t(
              input_field_name,
              RESTfulRoute(kRESTfulSchemaURLComponent, route_suffix, URLPathArgs::CountMask::None, handler)));
        });
  }

  template <typename FIELD_TYPE, typename ENTRY_TYPE_WRAPPER, typename GENERIC_HANDLER>
  void RegisterAdditionalFieldDataHandlers(const std::string& field_name,
                                           semantics::rest::RESTWithSingleKey,
                                           GENERIC_HANDLER generic_data_handler) {
    for (const auto& alternate_primary_key_suffix : {".key", ".0", ".K"}) {
      registerer(storage_handlers_map_entry_t(field_name,
                                              RESTfulRoute(kRESTfulDataURLComponent,
                                                           alternate_primary_key_suffix,
                                                           URLPathArgs::CountMask::Any,
                                                           generic_data_handler)));
    }
  }

  template <typename ENTRY_TYPE_WRAPPER, typename PARTIAL_KEY_OPERATION>
  std::function<void(Request)> GenerateRowOrColHandler(const std::string& field_name) {
    static_assert(std::is_same<PARTIAL_KEY_OPERATION, semantics::rest::operation::OnMatrixRow>::value ||
                      std::is_same<PARTIAL_KEY_OPERATION, semantics::rest::operation::OnMatrixCol>::value,
                  "");
    auto& storage = this->storage;
    const std::string restful_url_prefix = this->restful_url_prefix;

    using entry_t = typename ENTRY_TYPE_WRAPPER::entry_t;
    using key_t = typename ENTRY_TYPE_WRAPPER::key_t;

    return [&storage, restful_url_prefix, field_name](Request request) {
      // TODO(dkorolev): Pass `BorrowedWithCallback<Storage>` into the request handler.
      std::lock_guard<std::mutex> lock(storage.UnderlyingStream()->Impl()->publishing_mutex);
      auto generic_input = RESTfulGenericInput<STORAGE>(storage, restful_url_prefix);
      if (request.method == "GET") {
        DataHandlerImpl<GET, PARTIAL_KEY_OPERATION, specific_field_t, entry_t, key_t> handler;
        handler.Enter(
            std::move(request),
            // Capture by reference since this lambda is run synchronously.
            [&handler, &generic_input, &field_name](Request request, const Optional<std::string>& url_key) {
              const specific_field_t& field = generic_input.storage(::current::storage::ImmutableFieldByIndex<INDEX>());
              generic_input.storage.template ReadOnlyTransaction<current::locks::MutexLockStatus::AlreadyLocked>(
                                        // Capture local variables by value for safe async transactions.
                                        [handler, generic_input, &field, url_key, field_name](
                                            immutable_fields_t fields) -> Response {
                                          using RowColGETInput =
                                              RESTfulGETRowColInput<STORAGE,
                                                                    typename PARTIAL_KEY_OPERATION::key_completeness_t,
                                                                    specific_field_t>;
                                          const RowColGETInput input(
                                              std::move(generic_input), fields, field, field_name, url_key);
                                          return handler.Run(input);
                                        },
                                        std::move(request)).Detach();
            });
      } else {
        request(
            REST_IMPL::ErrorMethodNotAllowed(request.method, "Only GET method is allowed for partial key operations."));
      }
    };
  }

  template <typename FIELD_TYPE, typename ENTRY_TYPE_WRAPPER, typename GENERIC_HANDLER>
  void RegisterAdditionalFieldDataHandlers(const std::string& field_name,
                                           semantics::rest::RESTWithPairedKey,
                                           GENERIC_HANDLER unused_generic_data_handler) {
    static_cast<void>(unused_generic_data_handler);

    const std::string dot = ".";

    const auto row_handler =
        GenerateRowOrColHandler<ENTRY_TYPE_WRAPPER, semantics::rest::operation::OnMatrixRow>(field_name);
    const auto col_handler =
        GenerateRowOrColHandler<ENTRY_TYPE_WRAPPER, semantics::rest::operation::OnMatrixCol>(field_name);

    for (const auto& row_key_route_suffix : {"1", "row", "left", "l", "L"}) {
      registerer(storage_handlers_map_entry_t(field_name,
                                              RESTfulRoute(kRESTfulDataURLComponent,
                                                           dot + row_key_route_suffix,
                                                           URLPathArgs::CountMask::None | URLPathArgs::CountMask::One,
                                                           row_handler)));
    }
    for (const auto& col_key_route_suffix : {"2", "col", "right", "r", "R"}) {
      registerer(storage_handlers_map_entry_t(field_name,
                                              RESTfulRoute(kRESTfulDataURLComponent,
                                                           dot + col_key_route_suffix,
                                                           URLPathArgs::CountMask::None | URLPathArgs::CountMask::One,
                                                           col_handler)));
    }
  }
};

template <class REST_IMPL, int INDEX, typename STORAGE>
void GenerateRESTfulHandler(registerer_t registerer, STORAGE& storage, const std::string& restful_url_prefix) {
  storage(::current::storage::FieldNameAndTypeByIndex<INDEX>(),
          PerFieldRESTfulHandlerGenerator<REST_IMPL, INDEX, STORAGE>(registerer, storage, restful_url_prefix));
}

}  // namespace current::storage::rest::impl

template <class STORAGE_IMPL, class REST_IMPL = plain::Plain>
class RESTfulStorage {
 public:
  using immutable_fields_t = ImmutableFields<STORAGE_IMPL>;
  using mutable_fields_t = MutableFields<STORAGE_IMPL>;

  // TODO(dkorolev): `unique_ptr` and move semantics. Move to `-std=c++14` maybe? :-)
  // FIXME_DIMA <-- keep this marker for me to find it once we do move to C++14.
  using cqs_universal_parser_t = std::function<std::shared_ptr<CurrentStruct>(Request&)>;
  using cqs_query_handler_t = std::function<Response(
      immutable_fields_t, std::shared_ptr<CurrentStruct> command, const cqs::CQSParameters& cqs_parameters)>;
  using cqs_command_handler_t = std::function<Response(
      mutable_fields_t, std::shared_ptr<CurrentStruct> command, const cqs::CQSParameters& cqs_parameters)>;

  RESTfulStorage(STORAGE_IMPL& storage,
                 uint16_t port,
                 const std::string& route_prefix,
                 const std::string& restful_url_prefix_input)
      : data_(std::make_unique<Data>(port, route_prefix)) {
    const std::string restful_url_prefix = restful_url_prefix_input;
    if (!route_prefix.empty() && route_prefix.back() == '/') {
      CURRENT_THROW(current::Exception("`route_prefix` should not end with a slash."));  // LCOV_EXCL_LINE
    }

    // Fill in the map of `Storage field name` -> `HTTP handler`.
    ForEachFieldByIndex<void, STORAGE_IMPL::FIELDS_COUNT>::RegisterIt(storage, restful_url_prefix, data_->handlers_);

    // Register the CQS handlers as well.
    RegisterCQSHandlers(storage, restful_url_prefix);

    // Register handlers on a specific port under a specific path prefix.
    for (const auto& endpoint : data_->handlers_) {
      RegisterRoute(endpoint.first, endpoint.second);
    }

    std::set<std::string> fields_set;
    for (const auto& handler : data_->handlers_) {
      if (!handler.first.empty()) {
        // TODO(dkorolev): This is a hack, to make sure RESTful storage doesn't enumerate CQS routes in `/data/`.
        fields_set.insert(handler.first);
      }
    }
    RESTfulRegisterTopLevelInput<STORAGE_IMPL> input(storage,
                                                     restful_url_prefix,
                                                     port,
                                                     data_->handlers_scope_,
                                                     std::vector<std::string>(fields_set.begin(), fields_set.end()),
                                                     route_prefix.empty() ? "/" : route_prefix,
                                                     data_->up_status_);
    REST_IMPL::RegisterTopLevel(input);
  }

  // To enable exposing fields under different names / URLs.
  void RegisterAlias(const std::string& target, const std::string& alias_name) {
    const auto lower = data_->handlers_.lower_bound(target);
    const auto upper = data_->handlers_.upper_bound(target);
    if (upper == lower) {
      // LCOV_EXCL_START
      CURRENT_THROW(current::Exception("RESTfulStorage::RegisterAlias(), `" + target + "` is undefined."));
      // LCOV_EXCL_STOP
    } else {
      for (auto it = lower; it != upper; ++it) {
        RegisterRoute(alias_name, it->second);
      }
    }
  }

  template <class QUERY_IMPL>
  void AddCQSQuery(const std::string& query) {
    std::lock_guard<std::mutex> lock(data_->cqs_handlers_mutex_);
    if (data_->cqs_query_map_.count(query)) {
      CURRENT_THROW(current::Exception("RESTfulStorage::AddCQSQuery(), `" + query + "` is already registered."));
    }
    data_->cqs_query_map_[query] = std::make_pair(
        [](Request& request) -> std::shared_ptr<CurrentStruct> {
          return ParseCQSRequest<QUERY_IMPL, current::url::FillObjectMode::Forgiving>(request);
        },
        [query](immutable_fields_t fields,
                std::shared_ptr<CurrentStruct> type_erased_query,
                const cqs::CQSParameters& cqs_parameters) -> Response {
          // Invoke `.template Query<>(...)`, the handler implemented as part of the `QUERY_IMPL` class.
          // The instance of `QUERY_IMPL` is passed at runtime, in a type-erased fashion, as all possible
          // query types are not available at compile time. Hence, the query object itself is stored as a
          // smart pointer to the base class (returned from `ParseCQSRequest(...)`), and it should be cast
          // down to the respective `QUERY_IMPL` type from within the transaction.
          return dynamic_cast<QUERY_IMPL&>(*type_erased_query.get())
              .template Query<ImmutableFields<STORAGE_IMPL>>(fields, cqs_parameters);
        });
  }

  template <class COMMAND_IMPL>
  void AddCQSCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(data_->cqs_handlers_mutex_);
    if (data_->cqs_command_map_.count(command)) {
      CURRENT_THROW(current::Exception("RESTfulStorage::AddCQSCommand(), `" + command + "` is already registered."));
    }
    data_->cqs_command_map_[command] = std::make_pair(
        [](Request& request) -> std::shared_ptr<CurrentStruct> {
          return ParseCQSRequest<COMMAND_IMPL, current::url::FillObjectMode::Strict>(request);
        },
        [command](mutable_fields_t fields,
                  std::shared_ptr<CurrentStruct> type_erased_command,
                  const cqs::CQSParameters& cqs_parameters) -> Response {
          fields.SetTransactionMetaField("X-Current-CQS-Command", command);
          // Invoke `.template Command<>(...)`, the handler implemented as part of the `COMMAND_IMPL` class.
          // The instance of `COMMAND_IMPL` is passed at runtime, in a type-erased fashion, as all possible
          // command types are not available at compile time. Hence, the command object itself is stored as a
          // smart pointer to the base class (returned from `ParseCQSRequest(...)`), and it should be cast
          // down to the respective `COMMAND_IMPL` type from within the transaction.
          return dynamic_cast<COMMAND_IMPL&>(*type_erased_command.get())
              .template Command<MutableFields<STORAGE_IMPL>>(fields, cqs_parameters);
        });
  }

  // Support for graceful shutdown. Alpha.
  void SwitchHTTPEndpointsTo503s() {
    data_->up_status_ = false;
    for (auto& route : data_->handler_routes_) {
      HTTP(data_->port_)
          .template Register<ReRegisterRoute::SilentlyUpdateExisting>(route.first, route.second, Serve503);
    }
  }

 private:
  // Need an `std::unique_ptr<>` for the whole REST object to stay `std::move()`-able.
  struct Data {
    const uint16_t port_;
    const std::string route_prefix_;

    std::vector<std::pair<std::string, URLPathArgs::CountMask>> handler_routes_;
    impl::storage_handlers_map_t handlers_;
    HTTPRoutesScope handlers_scope_;

    std::atomic_bool up_status_;
    mutable std::mutex cqs_handlers_mutex_;
    std::unordered_map<std::string, std::pair<cqs_universal_parser_t, cqs_query_handler_t>> cqs_query_map_;
    std::unordered_map<std::string, std::pair<cqs_universal_parser_t, cqs_command_handler_t>> cqs_command_map_;

    Data(uint16_t port, const std::string& route_prefix) : port_(port), route_prefix_(route_prefix), up_status_(true) {}
  };
  std::unique_ptr<Data> data_;

  // The `BLAH` template parameter is required to fight the "explicit specialization in class scope" error.
  template <typename BLAH, int I>
  struct ForEachFieldByIndex {
    static void RegisterIt(STORAGE_IMPL& storage,
                           const std::string& restful_url_prefix,
                           impl::storage_handlers_map_t& handlers) {
      ForEachFieldByIndex<BLAH, I - 1>::RegisterIt(storage, restful_url_prefix, handlers);
      using specific_entry_type_t =
          typename impl::PerFieldRESTfulHandlerGenerator<REST_IMPL, I - 1, STORAGE_IMPL>::specific_entry_type_t;
      current::metaprogramming::CallIf<FieldExposedViaREST<STORAGE_IMPL, specific_entry_type_t>::exposed>::With([&] {
        const auto registerer =
            [&handlers](const impl::storage_handlers_map_entry_t& restful_route) { handlers.insert(restful_route); };
        impl::GenerateRESTfulHandler<REST_IMPL, I - 1, STORAGE_IMPL>(registerer, storage, restful_url_prefix);
      });
    }
  };

  template <typename BLAH>
  struct ForEachFieldByIndex<BLAH, 0> {
    static void RegisterIt(STORAGE_IMPL&, const std::string&, impl::storage_handlers_map_t&) {}
  };

  void RegisterRoute(const std::string& field_name, const RESTfulRoute& route) {
    const auto path = data_->route_prefix_ + '/' + route.resource_prefix +
                      (field_name.empty() ? "" : '/' + field_name) + route.resource_suffix;
    data_->handler_routes_.emplace_back(path, route.resource_args_mask);
    data_->handlers_scope_ += HTTP(data_->port_).Register(path, route.resource_args_mask, route.handler);
  }

  template <typename T, url::FillObjectMode MODE>
  static std::shared_ptr<CurrentStruct> ParseCQSRequest(Request& request) {
    try {
      auto object = std::make_shared<T>();
      if (reflection::FieldCounter<T>::value > 0) {
        if (!request.body.empty()) {
          ParseJSON<T, JSONFormat::Minimalistic>(request.body, *object);
        } else {
          request.url.query.FillObject<T, MODE>(*object);
        }
      }
      return object;
    } catch (const TypeSystemParseJSONException& e) {
      request(cqs::CQSParseJSONException(e.OriginalDescription()), HTTPResponseCode.BadRequest);
      return nullptr;
    } catch (const url::URLParseObjectAsURLParameterException& e) {
      request(cqs::CQSParseURLException(e.OriginalDescription()), HTTPResponseCode.BadRequest);
      return nullptr;
    }
  }
  void RegisterCQSHandlers(STORAGE_IMPL& storage, const std::string& restful_url_prefix) {
    const Data& data = *data_;

    const auto cqs_query_handler = [&data, &storage, restful_url_prefix](Request request) {
      std::lock_guard<std::mutex> lock(storage.UnderlyingStream()->Impl()->publishing_mutex);
      if (request.url_path_args.empty()) {
        request(Response(cqs::CQSHandlerNotSpecified(), HTTPResponseCode.NotFound));
      } else if (request.method != "GET") {
        request(REST_IMPL::ErrorMethodNotAllowed(request.method, "CQS queries must be GET-s."));
      } else {
        std::lock_guard<std::mutex> lock(data.cqs_handlers_mutex_);
        const auto cit = data.cqs_query_map_.find(request.url_path_args[0]);
        if (cit != data.cqs_query_map_.end()) {
          const auto& f_parse_query_body = cit->second.first;
          const auto& f_run_query = cit->second.second;
          auto generic_input = RESTfulGenericInput<STORAGE_IMPL>(storage, restful_url_prefix);
          using CQSHandlerImpl = typename REST_IMPL::template RESTfulCQSHandler<STORAGE_IMPL>;
          CQSHandlerImpl handler;
          std::shared_ptr<CurrentStruct> type_erased_query = f_parse_query_body(request);
          if (type_erased_query) {
            typename CQSHandlerImpl::Context context;
            handler.Enter(
                std::move(request),
                context,
                // Capture by reference since this lambda is run synchronously.
                [&handler, &f_run_query, &generic_input, &type_erased_query, &context](Request request) {
                  const STORAGE_IMPL& storage = generic_input.storage;
                  const cqs::CQSParameters cqs_parameters(generic_input.restful_url_prefix, request);
                  storage.template ReadOnlyTransaction<current::locks::MutexLockStatus::AlreadyLocked>(
                              // TODO(dkorolev): Revisit this as Owned/Borrowed are the organic part of Storage.
                              // Capture local variables by value for safe async transactions.
                              [&f_run_query, handler, cqs_parameters, type_erased_query, context](
                                  immutable_fields_t fields) -> Response {
                                return handler.RunQuery(
                                    context, f_run_query, fields, std::move(type_erased_query), cqs_parameters);
                              },
                              std::move(request)).Detach();
                });
          }
        } else {
          request(Response(cqs::CQSHandlerNotFound(), HTTPResponseCode.NotFound));
        }
      }
    };
    data_->handlers_.emplace("",
                             RESTfulRoute(kRESTfulCQSQueryURLComponent,
                                          "",
                                          URLPathArgs::CountMask::None | URLPathArgs::CountMask::One,
                                          cqs_query_handler));

    const auto cqs_command_handler = [&data, &storage, restful_url_prefix](Request request) {
      std::lock_guard<std::mutex> lock(storage.UnderlyingStream()->Impl()->publishing_mutex);
      if (!storage.template IsMasterStorage<current::locks::MutexLockStatus::AlreadyLocked>()) {
        request(Response(cqs::CQSCommandNeedsMasterStorage(), HTTPResponseCode.ServiceUnavailable));
      } else if (request.method != "POST" && request.method != "PUT" && request.method != "PATCH") {
        request(REST_IMPL::ErrorMethodNotAllowed(request.method, "CQS commands must be {POST|PUT|PATCH}-es."));
      } else if (request.url_path_args.empty()) {
        request(Response(cqs::CQSHandlerNotSpecified(), HTTPResponseCode.NotFound));
      } else {
        std::lock_guard<std::mutex> lock(data.cqs_handlers_mutex_);
        const auto cit = data.cqs_command_map_.find(request.url_path_args[0]);
        if (cit != data.cqs_command_map_.end()) {
          const auto& f_parse_command_body = cit->second.first;
          const auto& f_run_command = cit->second.second;
          auto generic_input = RESTfulGenericInput<STORAGE_IMPL>(storage, restful_url_prefix);
          using CQSHandlerImpl = typename REST_IMPL::template RESTfulCQSHandler<STORAGE_IMPL>;
          CQSHandlerImpl handler;
          std::shared_ptr<CurrentStruct> type_erased_command = f_parse_command_body(request);
          if (type_erased_command) {
            typename CQSHandlerImpl::Context ctx;
            handler.Enter(
                std::move(request),
                ctx,
                // Capture by reference since this lambda is run synchronously.
                [&handler, &f_run_command, &generic_input, &type_erased_command, &ctx](Request request) {
                  STORAGE_IMPL& storage = generic_input.storage;
                  const cqs::CQSParameters cqs_parameters(generic_input.restful_url_prefix, request);
                  storage.template ReadWriteTransaction<current::locks::MutexLockStatus::AlreadyLocked>(
                              // TODO(dkorolev): Revisit this as Owned/Borrowed are the organic part of Storage.
                              // Capture local variables by value for safe async transactions.
                              [&f_run_command, handler, cqs_parameters, type_erased_command, ctx](
                                  mutable_fields_t fields) -> Response {
                                return handler.RunCommand(
                                    ctx, f_run_command, fields, std::move(type_erased_command), cqs_parameters);
                              },
                              std::move(request)).Detach();
                });
          }
        } else {
          request(Response(cqs::CQSHandlerNotFound(), HTTPResponseCode.NotFound));
        }
      }
    };
    data_->handlers_.emplace("",
                             RESTfulRoute(kRESTfulCQSCommandURLComponent,
                                          "",
                                          URLPathArgs::CountMask::None | URLPathArgs::CountMask::One,
                                          cqs_command_handler));
  }
  static void Serve503(Request r) {
    r("{\"error\":\"In graceful shutdown mode. Come back soon.\"}\n", HTTPResponseCode.ServiceUnavailable);
  }
};

}  // namespace current::storage::rest
}  // namespace current::storage
}  // namespace current

using current::storage::rest::RESTfulStorage;

#endif  // CURRENT_STORAGE_REST_H
