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

#include "storage.h"

#include "rest/basic.h"

#include "../Blocks/HTTP/api.h"

namespace current {
namespace storage {
namespace rest {

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
  using T_SPECIFIC_FIELD = typename decltype(
      std::declval<STORAGE>()(::current::storage::FieldTypeExtractor<INDEX>()))::T_PARTICULAR_FIELD;

  template <class VERB, typename T1, typename T2, typename T3, typename T4>
  using CustomHandler = typename T_REST_IMPL::template RESTful<VERB, T1, T2, T3, T4>;

  STORAGE& storage;
  const std::string restful_url_prefix;

  RESTfulHandlerGenerator(STORAGE& storage, const std::string& restful_url_prefix)
      : storage(storage), restful_url_prefix(restful_url_prefix) {}

  template <typename FIELD_TYPE, typename ENTRY_TYPE_WRAPPER>
  STORAGE_HANDLERS_MAP_ENTRY operator()(const char* input_field_name, FIELD_TYPE, ENTRY_TYPE_WRAPPER) {
    auto& storage = this->storage;  // For lambdas.
    const std::string restful_url_prefix = this->restful_url_prefix;
    const std::string field_name = input_field_name;
    return STORAGE_HANDLERS_MAP_ENTRY(
        field_name,
        [&storage, restful_url_prefix, field_name](Request request) {
          if (request.method == "GET") {
            CustomHandler<GET,
                          T_IMMUTABLE_FIELDS,
                          T_SPECIFIC_FIELD,
                          typename ENTRY_TYPE_WRAPPER::T_ENTRY,
                          typename ENTRY_TYPE_WRAPPER::T_KEY> handler;
            handler.Enter(
                std::move(request),
                [&handler, &storage, &restful_url_prefix, field_name](Request request,
                                                                      const std::string& url_key) {
                  const T_SPECIFIC_FIELD& field = storage(::current::storage::ImmutableFieldByIndex<INDEX>());
                  storage.Transaction(
                              [handler, url_key, &storage, &field, &restful_url_prefix, field_name](
                                  T_IMMUTABLE_FIELDS fields) -> Response {
                                const struct {
                                  T_STORAGE& storage;
                                  T_IMMUTABLE_FIELDS fields;
                                  const T_SPECIFIC_FIELD& field;
                                  const std::string& url_key;
                                  const std::string& restful_url_prefix;
                                  const std::string& field_name;
                                } args{storage, fields, field, url_key, restful_url_prefix, field_name};
                                return handler.Run(args);
                              },
                              std::move(request)).Detach();
                });
          } else if (request.method == "POST") {
            CustomHandler<POST,
                          T_IMMUTABLE_FIELDS,
                          T_SPECIFIC_FIELD,
                          typename ENTRY_TYPE_WRAPPER::T_ENTRY,
                          typename ENTRY_TYPE_WRAPPER::T_KEY> handler;
            handler.Enter(
                std::move(request),
                [&handler, &storage, &restful_url_prefix, field_name](Request request) {
                  try {
                    auto mutable_entry = ParseJSON<typename ENTRY_TYPE_WRAPPER::T_ENTRY>(request.body);
                    T_SPECIFIC_FIELD& field = storage(::current::storage::MutableFieldByIndex<INDEX>());
                    storage.Transaction(
                                [handler, &storage, &field, mutable_entry, &restful_url_prefix, field_name](
                                    T_MUTABLE_FIELDS fields) mutable -> Response {
                                  const struct {
                                    T_STORAGE& storage;
                                    T_MUTABLE_FIELDS fields;
                                    T_SPECIFIC_FIELD& field;
                                    typename ENTRY_TYPE_WRAPPER::T_ENTRY& entry;
                                    const std::string& restful_url_prefix;
                                    const std::string& field_name;
                                  } args{storage, fields, field, mutable_entry, restful_url_prefix, field_name};
                                  return handler.Run(args);
                                },
                                std::move(request)).Detach();
                  } catch (const TypeSystemParseJSONException& e) {
                    request(handler.ErrorBadJSON(e.What()));
                  }
                });
          } else if (request.method == "PUT") {
            CustomHandler<PUT,
                          T_IMMUTABLE_FIELDS,
                          T_SPECIFIC_FIELD,
                          typename ENTRY_TYPE_WRAPPER::T_ENTRY,
                          typename ENTRY_TYPE_WRAPPER::T_KEY> handler;
            handler.Enter(
                std::move(request),
                [&handler, &storage, &restful_url_prefix, field_name](Request request,
                                                                      const std::string& key_as_string) {
                  try {
                    const auto url_key = FromString<typename ENTRY_TYPE_WRAPPER::T_KEY>(key_as_string);
                    const auto entry = ParseJSON<typename ENTRY_TYPE_WRAPPER::T_ENTRY>(request.body);
                    const auto entry_key = sfinae::GetKey(entry);
                    T_SPECIFIC_FIELD& field = storage(::current::storage::MutableFieldByIndex<INDEX>());
                    storage.Transaction(
                                [handler,
                                 &storage,
                                 &field,
                                 url_key,
                                 entry,
                                 entry_key,
                                 &restful_url_prefix,
                                 field_name](T_MUTABLE_FIELDS fields) -> Response {
                                  const struct {
                                    T_STORAGE& storage;
                                    T_MUTABLE_FIELDS fields;
                                    T_SPECIFIC_FIELD& field;
                                    const typename ENTRY_TYPE_WRAPPER::T_KEY& url_key;
                                    const typename ENTRY_TYPE_WRAPPER::T_ENTRY& entry;
                                    const typename ENTRY_TYPE_WRAPPER::T_KEY& entry_key;
                                    const std::string& restful_url_prefix;
                                    const std::string& field_name;
                                  } args{storage,
                                         fields,
                                         field,
                                         url_key,
                                         entry,
                                         entry_key,
                                         restful_url_prefix,
                                         field_name};
                                  return handler.Run(args);
                                },
                                std::move(request)).Detach();
                  } catch (const TypeSystemParseJSONException& e) {
                    request(handler.ErrorBadJSON(e.What()));
                  }
                });
          } else if (request.method == "DELETE") {
            CustomHandler<DELETE,
                          T_IMMUTABLE_FIELDS,
                          T_SPECIFIC_FIELD,
                          typename ENTRY_TYPE_WRAPPER::T_ENTRY,
                          typename ENTRY_TYPE_WRAPPER::T_KEY> handler;
            handler.Enter(std::move(request),
                          [&handler, &storage, &restful_url_prefix, field_name](
                              Request request, const std::string& key_as_string) {
                            const auto key = FromString<typename ENTRY_TYPE_WRAPPER::T_KEY>(key_as_string);
                            T_SPECIFIC_FIELD& field = storage(::current::storage::MutableFieldByIndex<INDEX>());
                            storage.Transaction(
                                        [handler, &storage, &field, key, &restful_url_prefix, field_name](
                                            T_MUTABLE_FIELDS fields) -> Response {
                                          const struct {
                                            T_STORAGE& storage;
                                            T_MUTABLE_FIELDS fields;
                                            T_SPECIFIC_FIELD& field;
                                            const typename ENTRY_TYPE_WRAPPER::T_KEY& key;
                                            const std::string& restful_url_prefix;
                                            const std::string& field_name;
                                          } args{storage, fields, field, key, restful_url_prefix, field_name};
                                          return handler.Run(args);
                                        },
                                        std::move(request)).Detach();
                          });
          } else {
            request(T_REST_IMPL::ErrorMethodNotAllowed());
          }
        });
  }
};

template <class REST_IMPL, int INDEX, typename STORAGE>
STORAGE_HANDLERS_MAP_ENTRY GenerateRESTfulHandler(STORAGE& storage, const std::string& restful_url_prefix) {
  return storage(::current::storage::FieldNameAndTypeByIndexAndReturn<INDEX, STORAGE_HANDLERS_MAP_ENTRY>(),
                 RESTfulHandlerGenerator<REST_IMPL, INDEX, STORAGE>(storage, restful_url_prefix));
}

}  // namespace impl

template <class T_STORAGE_IMPL, class T_REST_IMPL = Basic>
class RESTfulStorage {
 public:
  RESTfulStorage(T_STORAGE_IMPL& storage,
                 int port,
                 const std::string& path_prefix = "/api",
                 const std::string& restful_url_prefix_input = "")
      : port_(port), path_prefix_(path_prefix) {
    const std::string restful_url_prefix =
        restful_url_prefix_input.empty() ? "http://localhost:" + ToString(port) : restful_url_prefix_input;

    if (!path_prefix.empty() && path_prefix.back() == '/') {
      CURRENT_THROW(current::Exception("`path_prefix` should not end with a slash."));
    }
    // Fill in the map of `Storage field name` -> `HTTP handler`.
    ForEachFieldByIndex<void, T_STORAGE_IMPL::FieldsCount()>::RegisterIt(
        storage, restful_url_prefix, handlers_);
    // Register handlers on a specific port under a specific path prefix.
    for (const auto& handler : handlers_) {
      const std::string route = path_prefix + '/' + handler.first;
      handler_routes_.push_back(route);
      handlers_scope_ += HTTP(port).Register(
          route, URLPathArgs::CountMask::None | URLPathArgs::CountMask::One, handler.second);
    }

    std::vector<std::string> fields;
    for (const auto& handler : handlers_) {
      fields.push_back(handler.first);
    }
    T_REST_IMPL::RegisterTopLevel(
        handlers_scope_, fields, port, path_prefix.empty() ? "/" : path_prefix, restful_url_prefix);
  }

  // To enable exposing fields under different names / URLs.
  void RegisterAlias(const std::string& target, const std::string& alias_name) {
    const auto cit = handlers_.find(target);
    if (cit == handlers_.end()) {
      CURRENT_THROW(current::Exception("RESTfulStorage::RegisterAlias(), `" + target + "` is undefined."));
    }
    const std::string route = path_prefix_ + '/' + alias_name;
    handler_routes_.push_back(route);
    handlers_scope_ +=
        HTTP(port_).Register(route, URLPathArgs::CountMask::None | URLPathArgs::CountMask::One, cit->second);
  }

  // Support for graceful shutdown. Alpha.
  void SwitchHTTPEndpointsTo503s() {
    for (auto& route : handler_routes_) {
      HTTP(port_).template Register<ReRegisterRoute::SilentlyUpdateExisting>(
          route,
          URLPathArgs::CountMask::None | URLPathArgs::CountMask::One,
          Serve503);
    }
  }

 private:
  const int port_;
  const std::string path_prefix_;
  std::vector<std::string> handler_routes_;
  impl::STORAGE_HANDLERS_MAP handlers_;
  HTTPRoutesScope handlers_scope_;

  // The `BLAH` template parameter is required to fight the "explicit specialization in class scope" error.
  template <typename BLAH, int I>
  struct ForEachFieldByIndex {
    static void RegisterIt(T_STORAGE_IMPL& storage,
                           const std::string& restful_url_prefix,
                           impl::STORAGE_HANDLERS_MAP& handlers) {
      ForEachFieldByIndex<BLAH, I - 1>::RegisterIt(storage, restful_url_prefix, handlers);
      handlers.insert(
          impl::GenerateRESTfulHandler<T_REST_IMPL, I - 1, T_STORAGE_IMPL>(storage, restful_url_prefix));
    }
  };

  template <typename BLAH>
  struct ForEachFieldByIndex<BLAH, 0> {
    static void RegisterIt(T_STORAGE_IMPL&, const std::string&, impl::STORAGE_HANDLERS_MAP&) {}
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
