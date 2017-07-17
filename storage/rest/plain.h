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

// `Plain` is a boilerplate example of how to customize RESTful access to Current Storage.
// This basic implementation supports GET, POST, and DELETE, with rudimentary text-only error messages.

#ifndef CURRENT_STORAGE_REST_PLAIN_H
#define CURRENT_STORAGE_REST_PLAIN_H

#include "sfinae.h"

#include "../api_types.h"
#include "../storage.h"

#include "../../blocks/http/api.h"
#include "../../typesystem/schema/schema.h"

namespace current {
namespace storage {
namespace rest {
namespace plain {

struct Plain {
  template <class HTTP_VERB, typename OPERATION, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTfulDataHandler;

  template <class INPUT>
  static void RegisterTopLevel(const INPUT&) {}

  template <typename OPERATION, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTfulDataHandler<GET, OPERATION, PARTICULAR_FIELD, ENTRY, KEY> {
    template <typename F>
    void EnterByKeyCompletenessFamily(Request request,
                                      semantics::key_completeness::FullKey,
                                      semantics::key_completeness::DictionaryOrMatrixCompleteKey,
                                      F&& next) {
      field_type_dependent_t<PARTICULAR_FIELD>::CallWithOptionalKeyFromURL(std::move(request), std::forward<F>(next));
    }

    template <typename F, typename KEY_COMPLETENESS>
    void EnterByKeyCompletenessFamily(Request request,
                                      KEY_COMPLETENESS,
                                      semantics::key_completeness::MatrixHalfKey,
                                      F&& next) {
      key_type_dependent_t<semantics::primary_key::Key>::CallWithOptionalKeyFromURL(std::move(request),
                                                                                    std::forward<F>(next));
    }

    template <typename F>
    void Enter(Request request, F&& next) {
      EnterByKeyCompletenessFamily(std::move(request),
                                   typename OPERATION::key_completeness_t(),
                                   typename OPERATION::key_completeness_t::completeness_family_t(),
                                   std::forward<F>(next));
    }

    template <class FIELD>
    std::string RunIterate(const FIELD& field, semantics::primary_key::Key) const {
      std::ostringstream result;
      for (const auto& element : field) {
        // In plain "REST", which is mostly here for unit testing purposes, no URL-ifying is performed.
        result << current::ToString(current::storage::sfinae::GetKey(element)) << '\t' << JSON(element) << '\n';
      }
      return result.str();
    }

    template <class FIELD>
    std::string RunIterate(const FIELD& field, semantics::primary_key::RowCol) const {
      std::ostringstream result;
      for (const auto& element : field) {
        // Basic REST, mostly for unit testing purposes. No need to URL-ify plain text output.
        result << current::ToString(current::storage::sfinae::GetRow(element)) << '\t'
               << current::ToString(current::storage::sfinae::GetCol(element)) << '\t' << JSON(element) << '\n';
      }
      return result.str();
    }

    // TODO(dkorolev): Or can `FIELD_SEMANTICS` be hardcoded here?
    template <class INPUT, typename FIELD_SEMANTICS>
    Response RunForFullOrPartialKey(const INPUT& input,
                                    semantics::key_completeness::FullKey,
                                    FIELD_SEMANTICS,
                                    semantics::key_completeness::DictionaryOrMatrixCompleteKey) const {
      if (Exists(input.get_url_key)) {
        const auto key = field_type_dependent_t<PARTICULAR_FIELD>::template ParseURLKey<KEY>(Value(input.get_url_key));
        const ImmutableOptional<ENTRY> result = input.field[key];
        if (Exists(result)) {
          return Value(result);
        } else {
          return Response("Nope.\n", HTTPResponseCode.NotFound);
        }
      } else {
        return RunIterate(input.field, typename OPERATION::top_level_iterating_key_t());
      }
    }

    template <class INPUT, typename FIELD_SEMANTICS, typename KEY_COMPLETENESS>
    Response RunForFullOrPartialKey(const INPUT& input,
                                    KEY_COMPLETENESS,
                                    FIELD_SEMANTICS,
                                    semantics::key_completeness::MatrixHalfKey) const {
      if (Exists(input.rowcol_get_url_key)) {
        const auto row_or_col_key =
            current::FromString<typename MatrixContainerProxy<KEY_COMPLETENESS>::template entry_outer_key_t<ENTRY>>(
                Value(input.rowcol_get_url_key));
        const auto iterable =
            GenericMatrixIterator<KEY_COMPLETENESS, FIELD_SEMANTICS>::RowOrCol(input.field, row_or_col_key);
        if (!iterable.Empty()) {
          std::ostringstream result;
          for (const auto& e : iterable) {
            result << JSON(e) << '\n';
          }
          return result.str();
        } else {
          return Response("Nope.\n", HTTPResponseCode.NotFound);
        }
      } else {
        std::ostringstream result;
        const auto iterable = GenericMatrixIterator<KEY_COMPLETENESS, FIELD_SEMANTICS>::RowsOrCols(input.field);
        // Must use `begin()/end()` here, can not use a range-based for-loop, as
        // `OuterKeyForPartialHypermediaCollectionView` (or .key() FWIW -- D.K.) if only available
        // on the top-level iterator, not on its deferenced type.
        for (auto iterator = iterable.begin(); iterator != iterable.end(); ++iterator) {
          result << current::ToString(iterator.OuterKeyForPartialHypermediaCollectionView()) << '\t'
                 << (*iterator).TotalElementsForHypermediaCollectionView() << '\n';
        }
        return result.str();
      }
    }

    template <class INPUT>
    Response Run(const INPUT& input) const {
      return RunForFullOrPartialKey(input,
                                    typename INPUT::key_completeness_t(),
                                    typename INPUT::field_t::semantics_t(),
                                    typename INPUT::key_completeness_t::completeness_family_t());
    }
  };

  template <typename OPERATION, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTfulDataHandler<POST, OPERATION, PARTICULAR_FIELD, ENTRY, KEY> {
    template <typename F>
    void Enter(Request request, F&& next) {
      field_type_dependent_t<PARTICULAR_FIELD>::CallWithOrWithoutKeyFromURL(
          std::move(request),
          [](Request request, const typename field_type_dependent_t<PARTICULAR_FIELD>::url_key_t&) {
            request("Should not have resource key in the URL.\n", HTTPResponseCode.BadRequest);
          },
          std::forward<F>(next));
    }
    template <class INPUT>
    Response Run(const INPUT& input) const {
      return RunImpl<INPUT, sfinae::HasInitializeOwnKey<decltype(std::declval<INPUT>().entry)>(0)>(input);
    }
    template <class INPUT, bool B>
    ENABLE_IF<!B, Response> RunImpl(const INPUT& input) const {
      return Plain::ErrorMethodNotAllowed(
          "POST", "Storage field `" + input.field_name + "` does not support key initialization.");
    }
    template <class INPUT, bool B>
    ENABLE_IF<B, Response> RunImpl(const INPUT& input) const {
      input.entry.InitializeOwnKey();
      const auto entry_key = field_type_dependent_t<PARTICULAR_FIELD>::ExtractOrComposeKey(input.entry);
      if (input.overwrite || !Exists(input.field[entry_key])) {
        input.field.Add(input.entry);
        return Response(field_type_dependent_t<PARTICULAR_FIELD>::ComposeURLKey(entry_key), HTTPResponseCode.Created);
      } else {
        return Response("Already exists.\n", HTTPResponseCode.Conflict);  // LCOV_EXCL_LINE
      }
    }
    static Response ErrorBadJSON(const std::string&) { return Response("Bad JSON.\n", HTTPResponseCode.BadRequest); }
  };

  template <typename OPERATION, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTfulDataHandler<PUT, OPERATION, PARTICULAR_FIELD, ENTRY, KEY> {
    template <typename F>
    void Enter(Request request, F&& next) {
      field_type_dependent_t<PARTICULAR_FIELD>::CallWithKeyFromURL(std::move(request), std::forward<F>(next));
    }
    template <class INPUT>
    Response Run(const INPUT& input) const {
      if (input.entry_key == input.put_key) {
        const bool exists = Exists(input.field[input.entry_key]);
        input.field.Add(input.entry);
        if (exists) {
          return Response("Updated.\n", HTTPResponseCode.OK);
        } else {
          return Response("Created.\n", HTTPResponseCode.Created);
        }
      } else {
        return Response("The object key doesn't match the URL key.\n", HTTPResponseCode.BadRequest);
      }
    }
    // LCOV_EXCL_START
    static Response ErrorBadJSON(const std::string&) { return Response("Bad JSON.\n", HTTPResponseCode.BadRequest); }
    // LCOV_EXCL_STOP
  };

  template <typename OPERATION, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTfulDataHandler<PATCH, OPERATION, PARTICULAR_FIELD, ENTRY, KEY> {
    template <typename F>
    void Enter(Request request, F&& next) {
      field_type_dependent_t<PARTICULAR_FIELD>::CallWithKeyFromURL(std::move(request), std::forward<F>(next));
    }
    template <class INPUT>
    Response Run(const INPUT& input) const {
      const auto current = input.field[input.patch_key];
      if (Exists(current)) {
        auto value = Value(current);
        try {
          PatchObjectWithJSON(value, input.patch_body);
          const auto entry_key = field_type_dependent_t<PARTICULAR_FIELD>::ExtractOrComposeKey(value);
          if (entry_key != input.patch_key) {
            return Response("PATCH should not change the key.\n", HTTPResponseCode.BadRequest);
          } else {
            input.field.Add(value);
            return Response("Patched.\n", HTTPResponseCode.OK);
          }
        } catch (const TypeSystemParseJSONException&) {
          return Response("Bad JSON.\n", HTTPResponseCode.BadRequest);
        }
      } else {
        return Response("Nope.\n", HTTPResponseCode.NotFound);
      }
    }
  };

  template <typename OPERATION, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTfulDataHandler<DELETE, OPERATION, PARTICULAR_FIELD, ENTRY, KEY> {
    template <typename F>
    void Enter(Request request, F&& next) {
      field_type_dependent_t<PARTICULAR_FIELD>::CallWithKeyFromURL(std::move(request), std::forward<F>(next));
    }
    template <class INPUT>
    Response Run(const INPUT& input) const {
      input.field.Erase(input.delete_key);
      return Response("Deleted.\n", HTTPResponseCode.OK);
    }
  };

  template <typename STORAGE, typename ENTRY>
  struct RESTfulSchemaHandler {
    using storage_t = STORAGE;
    using entry_t = ENTRY;

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
                     underlying_type_schema.AddType<entry_t>();
                     r(underlying_type_schema.GetSchemaInfo().Describe<LANGUAGE>());
                   });
      }
    };

    void RegisterRoutes(const storage_t& storage, registerer_t registerer) {
      // The default implementation of schema generation doesn't need to access the `storage` itself.
      static_cast<void>(storage);
      // Top-level handler: Just the name of the `CURRENT_STRUCT`.
      registerer("", [](Request r) { r(reflection::CurrentTypeName<entry_t>()); });
      // Per-language handlers: For `/schema.*` routes export the schema in the respective language.
      LanguageIterator per_language(registerer);
      current::reflection::ForEachLanguage(per_language);
    }
  };

  template <typename STORAGE>
  struct RESTfulCQSHandler {
    struct Context {};

    template <typename F>
    void Enter(Request request, Context&, F&& next) {
      next(std::move(request));
    }

    Response RunQuery(
        const Context&,
        std::function<Response(ImmutableFields<STORAGE>, std::shared_ptr<CurrentStruct>, const std::string&)> f,
        ImmutableFields<STORAGE> fields,
        std::shared_ptr<CurrentStruct> type_erased_query,
        const std::string& restful_url_prefix) const {
      return f(fields, std::move(type_erased_query), restful_url_prefix);
    }

    Response RunCommand(
        const Context&,
        std::function<Response(MutableFields<STORAGE>, std::shared_ptr<CurrentStruct>, const std::string&)> f,
        MutableFields<STORAGE> fields,
        std::shared_ptr<CurrentStruct> type_erased_command,
        const std::string& restful_url_prefix) const {
      return f(fields, std::move(type_erased_command), restful_url_prefix);
    }
  };

  // LCOV_EXCL_START
  static Response ErrorMethodNotAllowed(const std::string& method, const std::string& error_message) {
    return Response("Method " + method + " not allowed. " + error_message + '\n', HTTPResponseCode.MethodNotAllowed);
  }
  // LCOV_EXCL_STOP
};

}  // namespace current::storage::rest::plain
}  // namespace current::storage::rest
}  // namespace current::storage
}  // namespace current

#endif  // CURRENT_STORAGE_REST_PLAIN_H
