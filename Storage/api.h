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
  template <typename T_STORAGE>                        \
  struct FieldExposedViaREST<T_STORAGE, field> {       \
    constexpr static bool exposed = false;             \
  };                                                   \
  }                                                    \
  }                                                    \
  }

namespace impl {

using STORAGE_HANDLERS_MAP = std::map<std::string, std::function<void(Request)>>;
using STORAGE_HANDLERS_MAP_ENTRY = std::pair<std::string, std::function<void(Request)>>;

template <class T_REST_IMPL, int INDEX, typename STORAGE>
struct RESTfulHandlerGenerator {
  using T_STORAGE = STORAGE;
  using T_IMMUTABLE_FIELDS = ImmutableFields<STORAGE>;
  using T_MUTABLE_FIELDS = MutableFields<STORAGE>;

  // Field accessor type for this `INDEX`.
  // The type of `data.x` from within a `Transaction()`, where `x` is the field corresponding to index `INDEX`.
  using T_SPECIFIC_FIELD_EXTRACTOR =
      decltype(std::declval<STORAGE>()(::current::storage::FieldTypeExtractor<INDEX>()));
  using T_SPECIFIC_FIELD = typename T_SPECIFIC_FIELD_EXTRACTOR::T_PARTICULAR_FIELD;

  using T_SPECIFIC_ENTRY_TYPE_EXTRACTOR =
      decltype(std::declval<STORAGE>()(::current::storage::FieldEntryTypeExtractor<INDEX>()));
  using T_SPECIFIC_ENTRY_TYPE = typename T_SPECIFIC_ENTRY_TYPE_EXTRACTOR::T_PARTICULAR_FIELD;

  template <class VERB, typename FIELD, typename ENTRY, typename KEY>
  using CustomHandler = typename T_REST_IMPL::template RESTful<VERB, FIELD, ENTRY, KEY>;

  STORAGE& storage;
  const std::string restful_url_prefix;
  const std::string data_url_component;

  RESTfulHandlerGenerator(STORAGE& storage,
                          const std::string& restful_url_prefix,
                          const std::string& data_url_component)
      : storage(storage), restful_url_prefix(restful_url_prefix), data_url_component(data_url_component) {}

  template <typename FIELD_TYPE, typename ENTRY_TYPE_WRAPPER>
  STORAGE_HANDLERS_MAP_ENTRY operator()(const char* input_field_name, FIELD_TYPE, ENTRY_TYPE_WRAPPER) {
    auto& storage = this->storage;  // For lambdas.
    const std::string restful_url_prefix = this->restful_url_prefix;
    const std::string data_url_component = this->data_url_component;
    const std::string field_name = input_field_name;

    using T_ENTRY = typename ENTRY_TYPE_WRAPPER::T_ENTRY;
    using T_KEY = typename ENTRY_TYPE_WRAPPER::T_KEY;
    using GETHandler = CustomHandler<GET, T_SPECIFIC_FIELD, T_ENTRY, T_KEY>;
    using POSTHandler = CustomHandler<POST, T_SPECIFIC_FIELD, T_ENTRY, T_KEY>;
    using PUTHandler = CustomHandler<PUT, T_SPECIFIC_FIELD, T_ENTRY, T_KEY>;
    using DELETEHandler = CustomHandler<DELETE, T_SPECIFIC_FIELD, T_ENTRY, T_KEY>;

    return STORAGE_HANDLERS_MAP_ENTRY(
        field_name,
        // Top-level capture by value to make own copy.
        [&storage, restful_url_prefix, field_name, data_url_component](Request request) {
          auto generic_input = RESTfulGenericInput<STORAGE>(storage, restful_url_prefix, data_url_component);
          if (request.method == "GET") {
            GETHandler handler;
            handler.Enter(
                std::move(request),
                // Capture by reference since this lambda is supposed to run synchronously.
                [&handler, &generic_input, &field_name](Request request, const std::string& url_key) {
                  const T_SPECIFIC_FIELD& field =
                      generic_input.storage(::current::storage::ImmutableFieldByIndex<INDEX>());

                  generic_input.storage.Transaction(
                                            // Capture local variables by value for safe async transactions.
                                            [handler, generic_input, &field, url_key, field_name](
                                                T_MUTABLE_FIELDS fields) -> Response {
                                              using GETInput = RESTfulGETInput<STORAGE, T_SPECIFIC_FIELD>;
                                              GETInput input(
                                                  std::move(generic_input), fields, field, field_name, url_key);
                                              return handler.Run(input);
                                            },
                                            std::move(request)).Detach();
                });
          } else if (request.method == "POST") {
            POSTHandler handler;
            handler.Enter(
                std::move(request),
                // Capture by reference since this lambda is supposed to run synchronously.
                [&handler, &generic_input, &field_name](Request request) {
                  try {
                    auto mutable_entry = ParseJSON<typename ENTRY_TYPE_WRAPPER::T_ENTRY>(request.body);
                    T_SPECIFIC_FIELD& field =
                        generic_input.storage(::current::storage::MutableFieldByIndex<INDEX>());
                    generic_input.storage.Transaction(
                                              // Capture local variables by value for safe async transactions.
                                              [handler, generic_input, &field, mutable_entry, field_name](
                                                  T_MUTABLE_FIELDS fields) mutable -> Response {
                                                using POSTInput =
                                                    RESTfulPOSTInput<STORAGE,
                                                                     T_SPECIFIC_FIELD,
                                                                     typename ENTRY_TYPE_WRAPPER::T_ENTRY>;
                                                POSTInput input(std::move(generic_input),
                                                                fields,
                                                                field,
                                                                field_name,
                                                                mutable_entry);
                                                return handler.Run(input);
                                              },
                                              std::move(request)).Detach();
                  } catch (const TypeSystemParseJSONException& e) {
                    request(handler.ErrorBadJSON(e.What()));
                  }
                });
          } else if (request.method == "PUT") {
            PUTHandler handler;
            handler.Enter(
                std::move(request),
                // Capture by reference since this lambda is supposed to run synchronously.
                [&handler, &generic_input, &field_name](Request request, const std::string& key_as_string) {
                  try {
                    const auto url_key = current::FromString<typename ENTRY_TYPE_WRAPPER::T_KEY>(key_as_string);
                    const auto entry = ParseJSON<typename ENTRY_TYPE_WRAPPER::T_ENTRY>(request.body);
                    const auto entry_key = PerStorageFieldType<T_SPECIFIC_FIELD>::ExtractOrComposeKey(entry);
                    T_SPECIFIC_FIELD& field =
                        generic_input.storage(::current::storage::MutableFieldByIndex<INDEX>());
                    generic_input.storage
                        .Transaction(
                             // Capture local variables by value for safe async transactions.
                             [handler, generic_input, &field, url_key, entry, entry_key, field_name](
                                 T_MUTABLE_FIELDS fields) -> Response {
                               using PUTInput = RESTfulPUTInput<STORAGE,
                                                                T_SPECIFIC_FIELD,
                                                                typename ENTRY_TYPE_WRAPPER::T_ENTRY,
                                                                typename ENTRY_TYPE_WRAPPER::T_KEY>;
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
          } else if (request.method == "DELETE") {
            DELETEHandler handler;
            handler.Enter(
                std::move(request),
                // Capture by reference since this lambda is supposed to run synchronously.
                [&handler, &generic_input, &field_name](Request request, const std::string& key_as_string) {
                  const auto key = current::FromString<typename ENTRY_TYPE_WRAPPER::T_KEY>(key_as_string);
                  T_SPECIFIC_FIELD& field =
                      generic_input.storage(::current::storage::MutableFieldByIndex<INDEX>());
                  generic_input.storage.Transaction(
                                            // Capture local variables by value for safe async transactions.
                                            [handler, generic_input, &field, key, field_name](
                                                T_MUTABLE_FIELDS fields) -> Response {
                                              using DELETEInput =
                                                  RESTfulDELETEInput<STORAGE,
                                                                     T_SPECIFIC_FIELD,
                                                                     typename ENTRY_TYPE_WRAPPER::T_KEY>;
                                              DELETEInput input(
                                                  std::move(generic_input), fields, field, field_name, key);
                                              return handler.Run(input);
                                            },
                                            std::move(request)).Detach();
                });
          } else {
            request(T_REST_IMPL::ErrorMethodNotAllowed(request.method));  // LCOV_EXCL_LINE
          }
        });
  }
};

template <class REST_IMPL, int INDEX, typename STORAGE>
STORAGE_HANDLERS_MAP_ENTRY GenerateRESTfulHandler(STORAGE& storage,
                                                  const std::string& restful_url_prefix,
                                                  const std::string& data_url_component) {
  return storage(
      ::current::storage::FieldNameAndTypeByIndexAndReturn<INDEX, STORAGE_HANDLERS_MAP_ENTRY>(),
      RESTfulHandlerGenerator<REST_IMPL, INDEX, STORAGE>(storage, restful_url_prefix, data_url_component));
}

}  // namespace impl

template <class T_STORAGE_IMPL, class T_REST_IMPL = Basic>
class RESTfulStorage {
 public:
  RESTfulStorage(T_STORAGE_IMPL& storage,
                 int port,
                 const std::string& route_prefix,
                 const std::string& restful_url_prefix_input,
                 const std::string& data_url_component = "data")
      : port_(port),
        up_status_(std::make_unique<std::atomic_bool>(true)),
        route_prefix_(route_prefix),
        data_url_component_(data_url_component) {
    const std::string restful_url_prefix = restful_url_prefix_input;
    if (!route_prefix.empty() && route_prefix.back() == '/') {
      CURRENT_THROW(current::Exception("`route_prefix` should not end with a slash."));  // LCOV_EXCL_LINE
    }
    // Fill in the map of `Storage field name` -> `HTTP handler`.
    ForEachFieldByIndex<void, T_STORAGE_IMPL::FIELDS_COUNT>::RegisterIt(
        storage, restful_url_prefix, data_url_component, handlers_);
    // Register handlers on a specific port under a specific path prefix.
    for (const auto& handler : handlers_) {
      const std::string route = route_prefix + '/' + data_url_component_ + '/' + handler.first;
      handler_routes_.push_back(route);
      handlers_scope_ += HTTP(port).Register(
          route, URLPathArgs::CountMask::None | URLPathArgs::CountMask::One, handler.second);
    }

    std::vector<std::string> fields;
    for (const auto& handler : handlers_) {
      fields.push_back(handler.first);
    }
    RESTfulRegisterTopLevelInput<T_STORAGE_IMPL> input(storage,
                                                       restful_url_prefix,
                                                       data_url_component_,
                                                       port,
                                                       handlers_scope_,
                                                       fields,
                                                       route_prefix.empty() ? "/" : route_prefix,
                                                       *up_status_);
    T_REST_IMPL::RegisterTopLevel(input);
  }

  // To enable exposing fields under different names / URLs.
  void RegisterAlias(const std::string& target, const std::string& alias_name) {
    const auto cit = handlers_.find(target);
    if (cit == handlers_.end()) {
      // LCOV_EXCL_START
      CURRENT_THROW(current::Exception("RESTfulStorage::RegisterAlias(), `" + target + "` is undefined."));
      // LCOV_EXCL_STOP
    }
    const std::string route = route_prefix_ + '/' + data_url_component_ + '/' + alias_name;
    handler_routes_.push_back(route);
    handlers_scope_ +=
        HTTP(port_).Register(route, URLPathArgs::CountMask::None | URLPathArgs::CountMask::One, cit->second);
  }

  // Support for graceful shutdown. Alpha.
  void SwitchHTTPEndpointsTo503s() {
    *up_status_ = false;
    for (auto& route : handler_routes_) {
      HTTP(port_).template Register<ReRegisterRoute::SilentlyUpdateExisting>(
          route, URLPathArgs::CountMask::None | URLPathArgs::CountMask::One, Serve503);
    }
  }

 private:
  const int port_;
  // Need an `std::unique_ptr<>` for the whole REST to stay `std::move()`-able.
  std::unique_ptr<std::atomic_bool> up_status_;
  const std::string route_prefix_;
  const std::string data_url_component_;
  std::vector<std::string> handler_routes_;
  impl::STORAGE_HANDLERS_MAP handlers_;
  HTTPRoutesScope handlers_scope_;

  // The `BLAH` template parameter is required to fight the "explicit specialization in class scope" error.
  template <typename BLAH, int I>
  struct ForEachFieldByIndex {
    static void RegisterIt(T_STORAGE_IMPL& storage,
                           const std::string& restful_url_prefix,
                           const std::string& data_url_component,
                           impl::STORAGE_HANDLERS_MAP& handlers) {
      ForEachFieldByIndex<BLAH, I - 1>::RegisterIt(storage, restful_url_prefix, data_url_component, handlers);
      using T_SPECIFIC_ENTRY_TYPE =
          typename impl::RESTfulHandlerGenerator<T_REST_IMPL, I - 1, T_STORAGE_IMPL>::T_SPECIFIC_ENTRY_TYPE;
      current::metaprogramming::CallIf<
          FieldExposedViaREST<T_STORAGE_IMPL, T_SPECIFIC_ENTRY_TYPE>::exposed>::With([&] {
        handlers.insert(impl::GenerateRESTfulHandler<T_REST_IMPL, I - 1, T_STORAGE_IMPL>(
            storage, restful_url_prefix, data_url_component));
      });
    }
  };

  template <typename BLAH>
  struct ForEachFieldByIndex<BLAH, 0> {
    static void RegisterIt(T_STORAGE_IMPL&,
                           const std::string&,
                           const std::string&,
                           impl::STORAGE_HANDLERS_MAP&) {}
  };

  static void Serve503(Request r) {
    r("{\"error\":\"In graceful shutdown mode. Come back soon.\"}\n", HTTPResponseCode.ServiceUnavailable);
  }
};

}  // namespace rest
}  // namespace storage
}  // namespace current

using current::storage::rest::RESTfulStorage;

#endif  // CURRENT_STORAGE_REST_H
