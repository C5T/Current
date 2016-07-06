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

// `Basic` is a boilerplate example of how to customize RESTful access to Current Storage.
// This basic implementation supports GET, POST, and DELETE, with rudimentary text-only messages on errors.

#ifndef CURRENT_STORAGE_REST_BASIC_H
#define CURRENT_STORAGE_REST_BASIC_H

#include "sfinae.h"

#include "../storage.h"

#include "../../Blocks/HTTP/api.h"
#include "../../TypeSystem/Schema/schema.h"

namespace current {
namespace storage {
namespace rest {

struct Basic {
  template <class HTTP_VERB, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTfulDataHandlerGenerator;

  template <class INPUT>
  static void RegisterTopLevel(const INPUT&) {}

  template <typename F_WITH, typename F_WITHOUT>
  static void WithOrWithoutKeyFromURL(Request request, F_WITH&& with, F_WITHOUT&& without) {
    if (request.url.query.has("key")) {
      with(std::move(request), request.url.query["key"]);
    } else if (!request.url_path_args.empty()) {
      with(std::move(request), request.url_path_args[0]);
    } else {
      without(std::move(request));
    }
  }

  template <typename F>
  static void WithKeyFromURL(Request request, F&& next_with_key) {
    WithOrWithoutKeyFromURL(
        std::move(request),
        next_with_key,
        [](Request request) { request("Need resource key in the URL.\n", HTTPResponseCode.BadRequest); });
  }

  template <typename F>
  static void WithOptionalKeyFromURL(Request request, F&& next) {
    WithOrWithoutKeyFromURL(
        std::move(request), next, [&next](Request request) { next(std::move(request), ""); });
  }

  template <typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTfulDataHandlerGenerator<GET, PARTICULAR_FIELD, ENTRY, KEY> {
    template <typename F>
    void Enter(Request request, F&& next) {
      WithOptionalKeyFromURL(std::move(request), std::forward<F>(next));
    }
    template <class INPUT>
    Response Run(const INPUT& input) const {
      if (!input.url_key.empty()) {
        const auto key = current::FromString<KEY>(input.url_key);
        const ImmutableOptional<ENTRY> result = input.field[key];
        if (Exists(result)) {
          return Value(result);
        } else {
          return Response("Nope.\n", HTTPResponseCode.NotFound);
        }
      } else {
        std::ostringstream result;
        for (const auto& element : PerStorageFieldType<PARTICULAR_FIELD>::Iterate(input.field)) {
          result << current::ToString(PerStorageFieldType<PARTICULAR_FIELD>::ExtractOrComposeKey(element))
                 << '\n';
        }
        return result.str();
      }
    }
  };

  template <typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTfulDataHandlerGenerator<POST, PARTICULAR_FIELD, ENTRY, KEY> {
    template <typename F>
    void Enter(Request request, F&& next) {
      if (!request.url_path_args.empty()) {
        request("Should not have resource key in the URL.\n", HTTPResponseCode.BadRequest);
      } else {
        next(std::move(request));
      }
    }
    template <class INPUT>
    Response Run(const INPUT& input) const {
      return RunImpl<INPUT, sfinae::HasInitializeOwnKey<decltype(std::declval<INPUT>().entry)>(0)>(input);
    }
    template <class INPUT, bool B>
    ENABLE_IF<!B, Response> RunImpl(const INPUT&) const {
      return Basic::ErrorMethodNotAllowed("POST");
    }
    template <class INPUT, bool B>
    ENABLE_IF<B, Response> RunImpl(const INPUT& input) const {
      input.entry.InitializeOwnKey();
      const auto entry_key = PerStorageFieldType<PARTICULAR_FIELD>::ExtractOrComposeKey(input.entry);
      if (!Exists(input.field[entry_key])) {
        input.field.Add(input.entry);
        return Response(current::ToString(entry_key), HTTPResponseCode.Created);
      } else {
        return Response("Already exists.\n", HTTPResponseCode.Conflict);  // LCOV_EXCL_LINE
      }
    }
    static Response ErrorBadJSON(const std::string&) {
      return Response("Bad JSON.\n", HTTPResponseCode.BadRequest);
    }
  };

  template <typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTfulDataHandlerGenerator<PUT, PARTICULAR_FIELD, ENTRY, KEY> {
    template <typename F>
    void Enter(Request request, F&& next) {
      WithKeyFromURL(std::move(request), std::forward<F>(next));
    }
    template <class INPUT>
    Response Run(const INPUT& input) const {
      if (input.entry_key == input.url_key) {
        const bool exists = Exists(input.field[input.entry_key]);
        input.field.Add(input.entry);
        if (exists) {
          return Response("Updated.\n", HTTPResponseCode.OK);
        } else {
          return Response("Created.\n", HTTPResponseCode.Created);
        }
      } else {
        return Response("Object key doesn't match URL key.\n", HTTPResponseCode.BadRequest);
      }
    }
    // LCOV_EXCL_START
    static Response ErrorBadJSON(const std::string&) {
      return Response("Bad JSON.\n", HTTPResponseCode.BadRequest);
    }
    // LCOV_EXCL_STOP
  };

  template <typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTfulDataHandlerGenerator<DELETE, PARTICULAR_FIELD, ENTRY, KEY> {
    template <typename F>
    void Enter(Request request, F&& next) {
      WithKeyFromURL(std::move(request), std::forward<F>(next));
    }
    template <class INPUT>
    Response Run(const INPUT& input) const {
      input.field.Erase(input.key);
      return Response("Deleted.\n", HTTPResponseCode.OK);
    }
  };

  template <typename ENTRY>
  class RESTfulSchemaHandlerGenerator {
   private:
    using registerer_t = std::function<void(const std::string& route_suffix, std::function<void(Request)>)>;
    struct LanguageIterator {
      registerer_t registerer;
      explicit LanguageIterator(registerer_t registerer) : registerer(registerer) {}
      template <current::reflection::Language LANGUAGE>
      void PerLanguage() {
        registerer('.' + current::ToString(LANGUAGE),
                   [](Request r) {
                     // TODO(dkorolev): Add caching one day.
                     reflection::StructSchema underlying_type_schema;
                     underlying_type_schema.AddType<ENTRY>();
                     r(underlying_type_schema.GetSchemaInfo().Describe<LANGUAGE>());
                   });
      }
    };

   public:
    void RegisterRoutes(registerer_t registerer) {
      // Top-level handler: Just the name of the `CURRENT_STRUCT`.
      registerer("", [](Request r) { r(reflection::CurrentTypeName<ENTRY>()); });
      // Per-language handlers: For `/schema.*` routes export the schema in the respective language.
      LanguageIterator per_language(registerer);
      current::reflection::ForEachLanguage(per_language);
    }
  };

  // LCOV_EXCL_START
  static Response ErrorMethodNotAllowed(const std::string& method) {
    return Response("Method " + method + " not allowed.\n", HTTPResponseCode.MethodNotAllowed);
  }
  // LCOV_EXCL_STOP
};

}  // namespace rest
}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_REST_BASIC_H
