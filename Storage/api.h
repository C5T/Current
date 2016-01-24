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

#include "storage.h"

#include "rest/basic.h"

#include "../Blocks/HTTP/api.h"

namespace current {
namespace storage {
namespace rest {

namespace impl {

template <class REST_IMPL, int INDEX, typename SERVER, typename STORAGE>
struct RESTfulStorageEndpointRegisterer {
  using T_STORAGE = STORAGE;
  using T_IMMUTABLE_FIELDS = ImmutableFields<STORAGE>;
  using T_MUTABLE_FIELDS = MutableFields<STORAGE>;

  // Field accessor type for this `INDEX`.
  // The type of `data.x` from within a `Transaction()`, where `x` is the field corresponding to index `INDEX`.
  using T_SPECIFIC_FIELD = typename decltype(
      std::declval<STORAGE>()(::current::storage::FieldTypeExtractor<INDEX>()))::T_PARTICULAR_FIELD;

  template <class VERB, typename T1, typename T2, typename T3, typename T4>
  using CustomHandler = typename REST_IMPL::template RESTful<VERB, T1, T2, T3, T4>;

  SERVER& server;
  STORAGE& storage;
  const std::string path_prefix;

  RESTfulStorageEndpointRegisterer(SERVER& server, STORAGE& storage, const std::string& path_prefix) : server(server), storage(storage), path_prefix(path_prefix) {}

  template <typename FIELD_TYPE, typename ENTRY_TYPE_WRAPPER>
  HTTPRoutesScopeEntry operator()(const char*, FIELD_TYPE, ENTRY_TYPE_WRAPPER) {
    auto& storage = this->storage;  // For lambdas.
    return server.Register(
        path_prefix + storage(::current::storage::FieldNameByIndex<INDEX>()),
        URLPathArgs::CountMask::None | URLPathArgs::CountMask::One,
        [&storage](Request request) {
          if (request.method == "GET") {
            CustomHandler<GET,
                          T_IMMUTABLE_FIELDS,
                          T_SPECIFIC_FIELD,
                          typename ENTRY_TYPE_WRAPPER::T_ENTRY,
                          typename ENTRY_TYPE_WRAPPER::T_KEY> handler;
            handler.Enter(
                std::move(request),
                [&handler, &storage](Request request) {
                  const auto& key_as_string = request.url_path_args[0];
                  const auto key = FromString<typename ENTRY_TYPE_WRAPPER::T_KEY>(key_as_string);
                  const T_SPECIFIC_FIELD& field = storage(::current::storage::ImmutableFieldByIndex<INDEX>());
                  storage.Transaction([handler, key, &storage, &field](T_IMMUTABLE_FIELDS fields) -> Response {
                    const struct {
                      T_STORAGE& storage;
                      T_IMMUTABLE_FIELDS fields;
                      const T_SPECIFIC_FIELD& field;
                      const typename ENTRY_TYPE_WRAPPER::T_KEY& key;
                    } args{storage, fields, field, key};
                    return handler.Run(args);
                  }, std::move(request)).Detach();
                });
          } else if (request.method == "POST") {
            CustomHandler<POST,
                          T_IMMUTABLE_FIELDS,
                          T_SPECIFIC_FIELD,
                          typename ENTRY_TYPE_WRAPPER::T_ENTRY,
                          typename ENTRY_TYPE_WRAPPER::T_KEY> handler;
            handler.Enter(
                std::move(request),
                [&handler, &storage](Request request) {
                  try {
                    const auto body = ParseJSON<typename ENTRY_TYPE_WRAPPER::T_ENTRY>(request.body);
                    T_SPECIFIC_FIELD& field = storage(::current::storage::MutableFieldByIndex<INDEX>());
                    storage.Transaction([handler, &storage, &field, body](T_MUTABLE_FIELDS fields) -> Response {
                      const struct {
                        T_STORAGE& storage;
                        T_MUTABLE_FIELDS fields;
                        T_SPECIFIC_FIELD& field;
                        const typename ENTRY_TYPE_WRAPPER::T_ENTRY& entry;
                      } args{storage, fields, field, body};
                      return handler.Run(args);
                    }, std::move(request)).Detach();
                  } catch (const TypeSystemParseJSONException&) {
                    request(handler.ErrorBadJSON());
                  }
                });
          } else if (request.method == "DELETE") {
            CustomHandler<DELETE,
                          T_IMMUTABLE_FIELDS,
                          T_SPECIFIC_FIELD,
                          typename ENTRY_TYPE_WRAPPER::T_ENTRY,
                          typename ENTRY_TYPE_WRAPPER::T_KEY> handler;
            handler.Enter(
                std::move(request),
                [&handler, &storage](Request request) {
                  const auto& key_as_string = request.url_path_args[0];
                  const auto key = FromString<typename ENTRY_TYPE_WRAPPER::T_KEY>(key_as_string);
                  T_SPECIFIC_FIELD& field = storage(::current::storage::MutableFieldByIndex<INDEX>());
                  storage.Transaction([handler, &storage, &field, key](T_MUTABLE_FIELDS fields) -> Response {
                    const struct {
                      T_STORAGE& storage;
                      T_MUTABLE_FIELDS fields;
                      T_SPECIFIC_FIELD& field;
                      const typename ENTRY_TYPE_WRAPPER::T_KEY& key;
                    } args{storage, fields, field, key};
                    return handler.Run(args);
                  }, std::move(request)).Detach();
                });
          } else {
            request(REST_IMPL::ErrorMethodNotAllowed());
          }
        });
  }
};

template <class REST_IMPL, int INDEX, typename SERVER, typename STORAGE>
HTTPRoutesScopeEntry RegisterRESTfulStorageEndpoint(SERVER& server, STORAGE& storage, const std::string& path_prefix) {
  return storage(::current::storage::FieldNameAndTypeByIndexAndReturn<INDEX, HTTPRoutesScopeEntry>(),
                 RESTfulStorageEndpointRegisterer<REST_IMPL, INDEX, SERVER, STORAGE>(server, storage, path_prefix));
}

}  // namespace impl

template <class T_STORAGE_IMPL, class T_REST_IMPL = BasicREST>
class RESTfulStorage {
 public:
  RESTfulStorage(T_STORAGE_IMPL& storage, int port, const std::string& path_prefix = "/api/") {
    ForEachFieldByIndex<T_REST_IMPL, T_STORAGE_IMPL::FieldsCount()>::RegisterIt(
        handlers_scope, HTTP(port), storage, path_prefix);
  }

 private:
  HTTPRoutesScope handlers_scope;

  template <class REST_IMPL, int I>
  struct ForEachFieldByIndex {
    template <typename T_SERVER, typename T_STORAGE>
    static void RegisterIt(HTTPRoutesScope& scope, T_SERVER& http_server, T_STORAGE& storage, const std::string& path_prefix) {
      ForEachFieldByIndex<REST_IMPL, I - 1>::RegisterIt(scope, http_server, storage, path_prefix);
      scope += impl::RegisterRESTfulStorageEndpoint<REST_IMPL, I - 1>(http_server, storage, path_prefix);
    }
  };

  template <class REST_IMPL>
  struct ForEachFieldByIndex<REST_IMPL, 0> {
    template <typename T_SERVER, typename T_STORAGE>
    static void RegisterIt(HTTPRoutesScope&, T_SERVER&, T_STORAGE&, const std::string&) {}
  };
};

}  // namespace rest
}  // namespace storage
}  // namespace current

using current::storage::rest::RESTfulStorage;

#endif  // CURRENT_STORAGE_REST_H
