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
  RESTfulHandlerGenerator(STORAGE& storage) : storage(storage) {}

  template <typename FIELD_TYPE, typename ENTRY_TYPE_WRAPPER>
  STORAGE_HANDLERS_MAP_ENTRY operator()(const char* field_name, FIELD_TYPE, ENTRY_TYPE_WRAPPER) {
    auto& storage = this->storage;  // For lambdas.
    return STORAGE_HANDLERS_MAP_ENTRY(
        field_name,
        [&storage](Request request) {
          if (request.method == "GET") {
            CustomHandler<GET,
                          T_IMMUTABLE_FIELDS,
                          T_SPECIFIC_FIELD,
                          typename ENTRY_TYPE_WRAPPER::T_ENTRY,
                          typename ENTRY_TYPE_WRAPPER::T_KEY> handler;
            handler.Enter(
                std::move(request),
                [&handler, &storage](Request request, const std::string& key_as_string) {
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
            handler.Enter(
                std::move(request),
                [&handler, &storage](Request request, const std::string& key_as_string) {
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
            request(T_REST_IMPL::ErrorMethodNotAllowed());
          }
        });
  }
};

template <class REST_IMPL, int INDEX, typename STORAGE>
STORAGE_HANDLERS_MAP_ENTRY GenerateRESTfulHandler(STORAGE& storage) {
  return storage(::current::storage::FieldNameAndTypeByIndexAndReturn<INDEX, STORAGE_HANDLERS_MAP_ENTRY>(),
                 RESTfulHandlerGenerator<REST_IMPL, INDEX, STORAGE>(storage));
}

}  // namespace impl

template <class T_STORAGE_IMPL, class T_REST_IMPL = Basic>
class RESTfulStorage {
 public:
  RESTfulStorage(T_STORAGE_IMPL& storage, int port, const std::string& path_prefix = "/api/")
      : port_(port), path_prefix_(path_prefix) {
    // Fill in the map of `Storage field name` -> `HTTP handler`.
    ForEachFieldByIndex<void, T_STORAGE_IMPL::FieldsCount()>::RegisterIt(storage, handlers_);
    // Register handlers on a specific port under a specific path prefix.
    for (const auto& handler : handlers_) {
      handlers_scope_ += HTTP(port).Register(path_prefix + handler.first,
                                             URLPathArgs::CountMask::None | URLPathArgs::CountMask::One,
                                             handler.second);
    }
  }

  // To enable exposing fields under different names / URLs.
  void RegisterAlias(const std::string& target, const std::string& alias_name) {
    const auto cit = handlers_.find(target);
    if (cit == handlers_.end()) {
      CURRENT_THROW(current::Exception("RESTfulStorage::RegisterAlias(), `" + target + "` is undefined."));
    }
    handlers_scope_ += HTTP(port_).Register(
        path_prefix_ + alias_name, URLPathArgs::CountMask::None | URLPathArgs::CountMask::One, cit->second);
  }

 private:
  const int port_;
  const std::string path_prefix_;
  impl::STORAGE_HANDLERS_MAP handlers_;
  HTTPRoutesScope handlers_scope_;

  // The `BLAH` template parameter is required to fight the "explicit specialization in class scope" error.
  template <typename BLAH, int I>
  struct ForEachFieldByIndex {
    static void RegisterIt(T_STORAGE_IMPL& storage, impl::STORAGE_HANDLERS_MAP& handlers) {
      ForEachFieldByIndex<BLAH, I - 1>::RegisterIt(storage, handlers);
      handlers.insert(impl::GenerateRESTfulHandler<T_REST_IMPL, I - 1, T_STORAGE_IMPL>(storage));
    }
  };

  template <typename BLAH>
  struct ForEachFieldByIndex<BLAH, 0> {
    static void RegisterIt(T_STORAGE_IMPL&, impl::STORAGE_HANDLERS_MAP&) {}
  };
};

}  // namespace rest
}  // namespace storage
}  // namespace current

using current::storage::rest::RESTfulStorage;

#endif  // CURRENT_STORAGE_REST_H
