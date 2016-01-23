/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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

#include "../Blocks/HTTP/api.h"

namespace current {
namespace storage {
namespace rest {
namespace impl {

struct BasicREST {
  template <typename HTTP_VERB, int INDEX, typename T>
  struct RESTfulWrapper;

  template <int INDEX, typename T>
  struct RESTfulWrapper<GET, INDEX, T> {
    template <typename KEY>
    struct Impl {
      const KEY& key;
      Response& response;
      Impl(const KEY& key, Response& response) : key(key), response(response) {}
      template <typename FIELD_DATA>
      void operator()(const FIELD_DATA& field_data) {
        const auto result = field_data[key];
        if (Exists(result)) {
          response = Value(result);
        } else {
          response = Response("nope", HTTPResponseCode.NotFound);
        }
      }
    };
    template <typename STORAGE, typename KEY>
    static Response Run(const STORAGE& storage, const KEY& key) {
      Response response;
      // NOTE: THIS IS VERY UNSAFE! `ImmutableFieldByIndex` assumes it is called from within a transaction
      // Better would be to move this call to support `data(...), and pass in `template
      // STORAGE_IMMUTABLE_FIELDS`.
      storage(::current::storage::ImmutableFieldByIndex<INDEX>(), Impl<KEY>(key, response));
      return response;
    }
  };

  template <int INDEX, typename T>
  struct RESTfulWrapper<POST, INDEX, T> {
    template <typename ENTRY>
    struct Impl {
      const ENTRY& entry;
      explicit Impl(const ENTRY& entry) : entry(entry) {}
      template <typename FIELD_DATA>
      void operator()(FIELD_DATA& field_data) {
        field_data.Add(entry);
      }
    };
    template <typename STORAGE, typename ENTRY>
    static void Run(STORAGE& storage, const ENTRY& entry) {
      // NOTE: THIS IS VERY UNSAFE! `MutableFieldByIndex` assumes it is called from within a transaction
      // Better would be to move this call to support `data(...), and pass in `template
      // STORAGE_IMMUTABLE_FIELDS`.
      storage(::current::storage::MutableFieldByIndex<INDEX>(), Impl<ENTRY>(entry));
    }
  };

  template <int INDEX, typename T>
  struct RESTfulWrapper<DELETE, INDEX, T> {
    template <typename KEY>
    struct Impl {
      const KEY& key;
      explicit Impl(const KEY& key) : key(key) {}
      template <typename FIELD_DATA>
      void operator()(FIELD_DATA& field_data) {
        field_data.Erase(key);
      }
    };
    template <typename STORAGE, typename KEY>
    static void Run(STORAGE& storage, const KEY& key) {
      // NOTE: THIS IS VERY UNSAFE! `MutableFieldByIndex` assumes it is called from within a transaction
      // Better would be to move this call to support `data(...), and pass in `template
      // STORAGE_IMMUTABLE_FIELDS`.
      storage(::current::storage::MutableFieldByIndex<INDEX>(), Impl<KEY>(key));
    }
  };
};

template <class REST_IMPL, int INDEX, typename SERVER, typename STORAGE>
struct RESTfulStorageEndpointRegisterer {
  template <class VERB, int I, typename T>
  using UserCode = typename REST_IMPL::template RESTfulWrapper<VERB, I, T>;

  SERVER& server;
  STORAGE& storage;
  RESTfulStorageEndpointRegisterer(SERVER& server, STORAGE& storage) : server(server), storage(storage) {}

  // TODO: The code below assumes FIELD_TYPE is `Dictionary` for now, which is not true in general.
  template <typename FIELD_TYPE, typename ENTRY_TYPE>
  HTTPRoutesScopeEntry operator()(const char*, FIELD_TYPE, ENTRY_TYPE) {
    auto& storage = this->storage;  // For lambdas.
    return server.Register("/api/" + storage(::current::storage::FieldNameByIndex<INDEX>()),
                           URLPathArgs::CountMask::None | URLPathArgs::CountMask::One,
                           [&storage](Request r) {
                             if (r.method == "GET") {
                               if (r.url_path_args.size() != 1) {
                                 r("TBD ERROR", HTTPResponseCode.BadRequest);
                               } else {
                                 const auto& key = r.url_path_args[0];
                                 storage.Transaction([key, &storage](ImmutableFields<STORAGE>) -> Response {
                                   return UserCode<GET, INDEX, typename ENTRY_TYPE::type>::Run(storage, key);
                                 }, std::move(r)).Wait();
                               }
                             } else if (r.method == "POST") {
                               if (!r.url_path_args.empty()) {
                                 r("TBD ERROR", HTTPResponseCode.BadRequest);
                               } else {
                                 try {
                                   const auto body = ParseJSON<typename ENTRY_TYPE::type>(r.body);
                                   storage.Transaction([body, &storage](MutableFields<STORAGE>) {
                                     UserCode<POST, INDEX, typename ENTRY_TYPE::type>::Run(storage, body);
                                   }).Wait();
                                   r("", HTTPResponseCode.NoContent);
                                 } catch (const TypeSystemParseJSONException&) {
                                   r("TBD ERROR", HTTPResponseCode.BadRequest);
                                 }
                               }
                             } else if (r.method == "DELETE") {
                               if (r.url_path_args.size() != 1) {
                                 r("TBD ERROR", HTTPResponseCode.BadRequest);
                               } else {
                                 const auto& key = r.url_path_args[0];
                                 storage.Transaction([key, &storage](MutableFields<STORAGE>) {
                                   UserCode<DELETE, INDEX, typename ENTRY_TYPE::type>::Run(storage, key);
                                 }).Wait();
                                 r("", HTTPResponseCode.NoContent);
                               }
                             } else {
                               r("", HTTPResponseCode.MethodNotAllowed);
                             }
                           });
  }
};

template <class REST_IMPL, int INDEX, typename SERVER, typename STORAGE>
HTTPRoutesScopeEntry RegisterRESTfulStorageEndpoint(SERVER& server, STORAGE& storage) {
  return storage(::current::storage::FieldNameAndTypeByIndexAndReturn<INDEX, HTTPRoutesScopeEntry>(),
                 RESTfulStorageEndpointRegisterer<REST_IMPL, INDEX, SERVER, STORAGE>(server, storage));
}

template <class, typename>
struct RegisterRESTfulStorageEndpoints;

template <class REST_IMPL, int N, int... NS>
struct RegisterRESTfulStorageEndpoints<REST_IMPL, current::variadic_indexes::indexes<N, NS...>> {
  template <typename T_SERVER, typename T_STORAGE>
  static void Run(HTTPRoutesScope& scope, T_SERVER& http_server, T_STORAGE& storage) {
    scope += RegisterRESTfulStorageEndpoint<REST_IMPL, N>(http_server, storage);
    RegisterRESTfulStorageEndpoints<REST_IMPL, current::variadic_indexes::indexes<NS...>>::Run(
        scope, http_server, storage);
  }
};

template <class REST_IMPL>
struct RegisterRESTfulStorageEndpoints<REST_IMPL, current::variadic_indexes::indexes<>> {
  template <typename T_SERVER, typename T_STORAGE>
  static void Run(HTTPRoutesScope&, T_SERVER&, T_STORAGE&) {}
};

}  // namespace impl

template <class T_STORAGE_IMPL, class T_REST_IMPL = impl::BasicREST>
struct RESTfulStorage {
  HTTPRoutesScope handlers_scope;
  RESTfulStorage(T_STORAGE_IMPL& storage, int port) {
    auto& http_server = HTTP(port);
    impl::RegisterRESTfulStorageEndpoints<
        T_REST_IMPL,
        current::variadic_indexes::generate_indexes<T_STORAGE_IMPL::FieldsCount()>>::Run(handlers_scope,
                                                                                         http_server,
                                                                                         storage);
  }
};

}  // namespace rest
}  // namespace storage
}  // namespace current

using current::storage::rest::RESTfulStorage;

using current::storage::rest::impl::BasicREST;

#endif  // CURRENT_STORAGE_REST_H
