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

#include "api_base.h"

#include "rest/basic.h"

#include "../TypeSystem/Reflection/reflection.h"
#include "../TypeSystem/Schema/schema.h"
#include "../Blocks/HTTP/api.h"
#include "../Bricks/template/call_if.h"

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

// RESTful routes per fields. Multimap to allow both `/data/$FIELD` and `/schema/$FIELD`.
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
  using specific_field_extractor_t =
      decltype(std::declval<STORAGE>()(::current::storage::FieldTypeExtractor<INDEX>()));
  using specific_field_t = typename specific_field_extractor_t::particular_field_t;

  using specific_entry_type_extractor_t =
      decltype(std::declval<STORAGE>()(::current::storage::FieldEntryTypeExtractor<INDEX>()));
  using specific_entry_type_t = typename specific_entry_type_extractor_t::particular_field_t;

  template <class VERB, typename FIELD, typename ENTRY, typename KEY>
  using DataHandlerImpl = typename REST_IMPL::template RESTfulDataHandlerGenerator<VERB, FIELD, ENTRY, KEY>;

  template <typename ENTRY>
  using SchemaHandlerImpl = typename REST_IMPL::template RESTfulSchemaHandlerGenerator<ENTRY>;

  const registerer_t registerer;
  STORAGE& storage;
  const std::string restful_url_prefix;
  const std::string data_url_component;
  const std::string schema_url_component;

  PerFieldRESTfulHandlerGenerator(registerer_t registerer,
                                  STORAGE& storage,
                                  const std::string& restful_url_prefix,
                                  const std::string& data_url_component,
                                  const std::string& schema_url_component)
      : registerer(registerer),
        storage(storage),
        restful_url_prefix(restful_url_prefix),
        data_url_component(data_url_component),
        schema_url_component(schema_url_component) {}

  template <typename FIELD_TYPE, typename ENTRY_TYPE_WRAPPER>
  void operator()(const char* input_field_name, FIELD_TYPE, ENTRY_TYPE_WRAPPER) {
    auto& storage = this->storage;  // For lambdas.
    const std::string restful_url_prefix = this->restful_url_prefix;
    const std::string data_url_component = this->data_url_component;
    const std::string schema_url_component = this->schema_url_component;
    const std::string field_name = input_field_name;

    // Data handler.
    using entry_t = typename ENTRY_TYPE_WRAPPER::entry_t;
    using key_t = typename ENTRY_TYPE_WRAPPER::key_t;
    using GETHandler = DataHandlerImpl<GET, specific_field_t, entry_t, key_t>;
    using POSTHandler = DataHandlerImpl<POST, specific_field_t, entry_t, key_t>;
    using PUTHandler = DataHandlerImpl<PUT, specific_field_t, entry_t, key_t>;
    using DELETEHandler = DataHandlerImpl<DELETE, specific_field_t, entry_t, key_t>;

    using SchemaRoutesGenerator = SchemaHandlerImpl<entry_t>;

    registerer(storage_handlers_map_entry_t(
        field_name,
        RESTfulRoute(
            data_url_component,
            "",
            URLPathArgs::CountMask::None | URLPathArgs::CountMask::One,
            // Top-level capture by value to make own copy.
            [&storage, restful_url_prefix, field_name, data_url_component, schema_url_component](
                Request request) {
              auto generic_input = RESTfulGenericInput<STORAGE>(
                  storage, restful_url_prefix, data_url_component, schema_url_component);
              if (request.method == "GET") {
                GETHandler handler;
                const bool export_requested = request.url.query.has("export");
                handler.Enter(
                    std::move(request),
                    // Capture by reference since this lambda is supposed to run synchronously.
                    [&storage, &handler, &generic_input, &field_name, export_requested](
                        Request request, const std::string& url_key) {
                      const specific_field_t& field =
                          generic_input.storage(::current::storage::ImmutableFieldByIndex<INDEX>());
                      generic_input.storage.ReadOnlyTransaction(
                                                // Capture local variables by value for safe async transactions.
                                                [&storage,
                                                 handler,
                                                 generic_input,
                                                 &field,
                                                 url_key,
                                                 field_name,
                                                 export_requested](immutable_fields_t fields) -> Response {
                                                  using GETInput = RESTfulGETInput<STORAGE, specific_field_t>;
                                                  GETInput input(std::move(generic_input),
                                                                 fields,
                                                                 field,
                                                                 field_name,
                                                                 url_key,
                                                                 storage.GetRole(),
                                                                 export_requested);
                                                  return handler.Run(input);
                                                },
                                                std::move(request)).Detach();
                    });
              } else if (request.method == "POST" && storage.GetRole() == StorageRole::Master) {
                POSTHandler handler;
                handler.Enter(
                    std::move(request),
                    // Capture by reference since this lambda is supposed to run synchronously.
                    [&handler, &generic_input, &field_name](Request request) {
                      try {
                        auto mutable_entry = ParseJSON<entry_t>(request.body);
                        specific_field_t& field =
                            generic_input.storage(::current::storage::MutableFieldByIndex<INDEX>());
                        generic_input.storage
                            .ReadWriteTransaction(
                                 // Capture local variables by value for safe async transactions.
                                 [handler, generic_input, &field, mutable_entry, field_name](
                                     mutable_fields_t fields) mutable -> Response {
                                   using POSTInput = RESTfulPOSTInput<STORAGE, specific_field_t, entry_t>;
                                   POSTInput input(
                                       std::move(generic_input), fields, field, field_name, mutable_entry);
                                   return handler.Run(input);
                                 },
                                 std::move(request))
                            .Detach();
                      } catch (const TypeSystemParseJSONException& e) {
                        request(handler.ErrorBadJSON(e.What()));
                      }
                    });
              } else if (request.method == "PUT" && storage.GetRole() == StorageRole::Master) {
                PUTHandler handler;
                handler.Enter(
                    std::move(request),
                    // Capture by reference since this lambda is supposed to run synchronously.
                    [&handler, &generic_input, &field_name](Request request, const std::string& key_as_string) {
                      try {
                        const auto url_key =
                            current::FromString<typename ENTRY_TYPE_WRAPPER::key_t>(key_as_string);
                        const auto entry = ParseJSON<entry_t>(request.body);
                        const auto entry_key =
                            PerStorageFieldType<specific_field_t>::ExtractOrComposeKey(entry);
                        specific_field_t& field =
                            generic_input.storage(::current::storage::MutableFieldByIndex<INDEX>());
                        generic_input.storage
                            .ReadWriteTransaction(
                                 // Capture local variables by value for safe async transactions.
                                 [handler, generic_input, &field, url_key, entry, entry_key, field_name](
                                     mutable_fields_t fields) -> Response {
                                   using PUTInput = RESTfulPUTInput<STORAGE,
                                                                    specific_field_t,
                                                                    entry_t,
                                                                    typename ENTRY_TYPE_WRAPPER::key_t>;
                                   PUTInput input(std::move(generic_input),
                                                  fields,
                                                  field,
                                                  field_name,
                                                  url_key,
                                                  entry,
                                                  entry_key);
                                   return handler.Run(input);
                                 },
                                 std::move(request))
                            .Detach();
                      } catch (const TypeSystemParseJSONException& e) {  // LCOV_EXCL_LINE
                        request(handler.ErrorBadJSON(e.What()));         // LCOV_EXCL_LINE
                      }
                    });
              } else if (request.method == "DELETE" && storage.GetRole() == StorageRole::Master) {
                DELETEHandler handler;
                handler.Enter(
                    std::move(request),
                    // Capture by reference since this lambda is supposed to run synchronously.
                    [&handler, &generic_input, &field_name](Request request, const std::string& key_as_string) {
                      const auto key = current::FromString<typename ENTRY_TYPE_WRAPPER::key_t>(key_as_string);
                      specific_field_t& field =
                          generic_input.storage(::current::storage::MutableFieldByIndex<INDEX>());
                      generic_input.storage.ReadWriteTransaction(
                                                // Capture local variables by value for safe async transactions.
                                                [handler, generic_input, &field, key, field_name](
                                                    mutable_fields_t fields) -> Response {
                                                  using DELETEInput =
                                                      RESTfulDELETEInput<STORAGE,
                                                                         specific_field_t,
                                                                         typename ENTRY_TYPE_WRAPPER::key_t>;
                                                  DELETEInput input(
                                                      std::move(generic_input), fields, field, field_name, key);
                                                  return handler.Run(input);
                                                },
                                                std::move(request)).Detach();
                    });
              } else {
                request(REST_IMPL::ErrorMethodNotAllowed(request.method));  // LCOV_EXCL_LINE
              }
            })));

    // Schema handlers.
    SchemaRoutesGenerator schema_routes_generator;
    schema_routes_generator.RegisterRoutes(
        [&](const std::string& extension, const std::function<void(Request)> handler) {
          registerer(storage_handlers_map_entry_t(
              input_field_name,
              RESTfulRoute(schema_url_component, extension, URLPathArgs::CountMask::None, handler)));
        });
  }
};

template <class REST_IMPL, int INDEX, typename STORAGE>
void GenerateRESTfulHandler(registerer_t registerer,
                            STORAGE& storage,
                            const std::string& restful_url_prefix,
                            const std::string& data_url_component,
                            const std::string& schema_url_component) {
  storage(::current::storage::FieldNameAndTypeByIndex<INDEX>(),
          PerFieldRESTfulHandlerGenerator<REST_IMPL, INDEX, STORAGE>(
              registerer, storage, restful_url_prefix, data_url_component, schema_url_component));
}

}  // namespace current::storage::impl

template <class STORAGE_IMPL, class REST_IMPL = Basic>
class RESTfulStorage {
 public:
  RESTfulStorage(STORAGE_IMPL& storage,
                 int port,
                 const std::string& route_prefix,
                 const std::string& restful_url_prefix_input,
                 const std::string& data_url_component = "data",
                 const std::string& schema_url_component = "schema")
      : port_(port),
        up_status_(std::make_unique<std::atomic_bool>(true)),
        route_prefix_(route_prefix),
        data_url_component_(data_url_component),
        schema_url_component_(schema_url_component) {
    const std::string restful_url_prefix = restful_url_prefix_input;
    if (!route_prefix.empty() && route_prefix.back() == '/') {
      CURRENT_THROW(current::Exception("`route_prefix` should not end with a slash."));  // LCOV_EXCL_LINE
    }
    // Fill in the map of `Storage field name` -> `HTTP handler`.
    ForEachFieldByIndex<void, STORAGE_IMPL::FIELDS_COUNT>::RegisterIt(
        storage, restful_url_prefix, data_url_component, schema_url_component, handlers_);
    // Register handlers on a specific port under a specific path prefix.
    for (const auto& endpoint : handlers_) {
      RegisterRoute(endpoint.first, endpoint.second);
    }

    std::set<std::string> fields_set;
    for (const auto& handler : handlers_) {
      fields_set.insert(handler.first);
    }
    RESTfulRegisterTopLevelInput<STORAGE_IMPL> input(
        storage,
        restful_url_prefix,
        data_url_component_,
        schema_url_component_,
        port,
        handlers_scope_,
        std::vector<std::string>(fields_set.begin(), fields_set.end()),
        route_prefix.empty() ? "/" : route_prefix,
        *up_status_);
    REST_IMPL::RegisterTopLevel(input);
  }

  // To enable exposing fields under different names / URLs.
  void RegisterAlias(const std::string& target, const std::string& alias_name) {
    const auto lower = handlers_.lower_bound(target);
    const auto upper = handlers_.upper_bound(target);
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

  // Support for graceful shutdown. Alpha.
  void SwitchHTTPEndpointsTo503s() {
    *up_status_ = false;
    for (auto& route : handler_routes_) {
      HTTP(port_)
          .template Register<ReRegisterRoute::SilentlyUpdateExisting>(route.first, route.second, Serve503);
    }
  }

 private:
  const int port_;
  // Need an `std::unique_ptr<>` for the whole REST to stay `std::move()`-able.
  std::unique_ptr<std::atomic_bool> up_status_;
  const std::string route_prefix_;
  const std::string data_url_component_;
  const std::string schema_url_component_;
  std::vector<std::pair<std::string, URLPathArgs::CountMask>> handler_routes_;
  impl::storage_handlers_map_t handlers_;
  HTTPRoutesScope handlers_scope_;

  // The `BLAH` template parameter is required to fight the "explicit specialization in class scope" error.
  template <typename BLAH, int I>
  struct ForEachFieldByIndex {
    static void RegisterIt(STORAGE_IMPL& storage,
                           const std::string& restful_url_prefix,
                           const std::string& data_url_component,
                           const std::string& schema_url_component,
                           impl::storage_handlers_map_t& handlers) {
      ForEachFieldByIndex<BLAH, I - 1>::RegisterIt(
          storage, restful_url_prefix, data_url_component, schema_url_component, handlers);
      using specific_entry_type_t =
          typename impl::PerFieldRESTfulHandlerGenerator<REST_IMPL, I - 1, STORAGE_IMPL>::specific_entry_type_t;
      current::metaprogramming::CallIf<FieldExposedViaREST<STORAGE_IMPL, specific_entry_type_t>::exposed>::With(
          [&] {
            const auto registerer = [&handlers](const impl::storage_handlers_map_entry_t& restful_route) {
              handlers.insert(restful_route);
            };
            impl::GenerateRESTfulHandler<REST_IMPL, I - 1, STORAGE_IMPL>(
                registerer, storage, restful_url_prefix, data_url_component, schema_url_component);
          });
    }
  };

  template <typename BLAH>
  struct ForEachFieldByIndex<BLAH, 0> {
    static void RegisterIt(STORAGE_IMPL&,
                           const std::string&,
                           const std::string&,
                           const std::string&,
                           impl::storage_handlers_map_t&) {}
  };

  void RegisterRoute(const std::string& field_name, const RESTfulRoute& route) {
    const auto path = route_prefix_ + '/' + route.resource_prefix + '/' + field_name + route.resource_suffix;
    handler_routes_.emplace_back(path, route.resource_args_mask);
    handlers_scope_ += HTTP(port_).Register(path, route.resource_args_mask, route.handler);
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
