/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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

// Advanced hypermedia: A rather hacky solution for Hypermedia REST API supporting:
// * Rich JSON format (top-level `url_*` fields, and actual data in `data`.)
// * Full and brief fields sets.
// * Poor man's stateless "pagination" through collections (`Dictionary` only support for now.)

#ifndef CURRENT_STORAGE_REST_ADVANCED_HYPERMEDIA_H
#define CURRENT_STORAGE_REST_ADVANCED_HYPERMEDIA_H

#include "hypermedia.h"
#include "sfinae.h"

#include "../api_types.h"

namespace current {
namespace storage {
namespace rest {

CURRENT_STRUCT_T(AdvancedHypermediaRESTRecordResponse) {
  CURRENT_FIELD(success, Optional<bool>);
  CURRENT_FIELD(url, std::string);
  CURRENT_FIELD(url_full, std::string);
  CURRENT_FIELD(url_brief, std::string);
  CURRENT_FIELD(url_directory, std::string);
  CURRENT_FIELD(data, T);
};

CURRENT_STRUCT_T(AdvancedHypermediaRESTContainerResponse) {
  CURRENT_FIELD(success, bool, true);
  CURRENT_FIELD(url, std::string);
  CURRENT_FIELD(url_directory, std::string);
  CURRENT_FIELD(i, uint64_t);
  CURRENT_FIELD(n, uint64_t);
  CURRENT_FIELD(total, uint64_t);
  CURRENT_FIELD(url_next_page, Optional<std::string>);
  CURRENT_FIELD(url_previous_page, Optional<std::string>);
  CURRENT_FIELD(data, std::vector<T>);
};

template <typename T, typename INPUT, typename TT>
inline AdvancedHypermediaRESTRecordResponse<T> FormatAsAdvancedHypermediaRecord(TT& record,
                                                                                const INPUT& input,
                                                                                bool set_success = true) {
  using particular_field_t = current::decay<decltype(input.field)>;

  AdvancedHypermediaRESTRecordResponse<T> response;
  const std::string key_as_url_string = field_type_dependent_t<particular_field_t>::ComposeURLKey(
      field_type_dependent_t<particular_field_t>::ExtractOrComposeKey(record));
  if (set_success) {
    response.success = true;
  }
  response.url_directory = input.restful_url_prefix + '/' + kRESTfulDataURLComponent + '/' + input.field_name;
  response.url = response.url_directory + '/' + key_as_url_string;
  response.url_full = response.url;
  response.url_brief = response.url + "?fields=brief";
  response.data = static_cast<const T&>(record);
  return response;
}

struct AdvancedHypermedia : Hypermedia {
  using SUPER_HYPERMEDIA = Hypermedia;

  template <class HTTP_VERB, typename OPERATION, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTfulDataHandlerGenerator
      : SUPER_HYPERMEDIA::RESTfulDataHandlerGenerator<HTTP_VERB, OPERATION, PARTICULAR_FIELD, ENTRY, KEY> {};

  template <typename STORAGE, typename ENTRY>
  using RESTfulSchemaHandlerGenerator = SUPER_HYPERMEDIA::RESTfulSchemaHandlerGenerator<STORAGE, ENTRY>;

  template <typename OPERATION, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTfulDataHandlerGenerator<GET, OPERATION, PARTICULAR_FIELD, ENTRY, KEY>
      : SUPER_HYPERMEDIA::RESTfulDataHandlerGenerator<GET, OPERATION, PARTICULAR_FIELD, ENTRY, KEY> {
    using SUPER_GET_HANDLER =
        SUPER_HYPERMEDIA::RESTfulDataHandlerGenerator<GET, OPERATION, PARTICULAR_FIELD, ENTRY, KEY>;

    using brief_entry_t = sfinae::brief_of_t<ENTRY>;

    // For per-record view, whether a full or brief format should be used.
    bool brief = false;

    // For poor man's pagination when viewing the collection.
    mutable uint64_t query_i = 0u;
    mutable uint64_t query_n = 10u;  // Default page size.

    template <typename F>
    void Enter(Request request, F&& next) {
      const auto& q = request.url.query;
      brief = (q["fields"] == "brief");
      query_i = current::FromString<uint64_t>(q.get("i", current::ToString(query_i)));
      query_n = current::FromString<uint64_t>(q.get("n", current::ToString(query_n)));
      SUPER_GET_HANDLER::Enter(std::move(request), std::forward<F>(next));
    }

    template <class INPUT, typename FIELD_SEMANTICS>
    Response RunForFullOrPartialKey(const INPUT& input,
                                    semantics::key_completeness::FullKey,
                                    FIELD_SEMANTICS,
                                    semantics::key_completeness::DictionaryOrMatrixCompleteKey) const {
      if (Exists(input.get_url_key)) {
        // Single record view.
        const auto url_key_value = Value(input.get_url_key);
        const auto entry_key =
            field_type_dependent_t<PARTICULAR_FIELD>::template ParseURLKey<KEY>(url_key_value);
        const ImmutableOptional<ENTRY> result = input.field[entry_key];
        if (Exists(result)) {
          const auto& value = Value(result);
          if (!input.export_requested) {
            auto response =
                (brief ? Response(FormatAsAdvancedHypermediaRecord<brief_entry_t>(value, input),
                                  HTTPResponseCode.OK)
                       : Response(FormatAsAdvancedHypermediaRecord<ENTRY>(value, input), HTTPResponseCode.OK));
            const auto last_modified = input.field.LastModified(entry_key);
            if (Exists(last_modified)) {
              response.SetHeader("Last-Modified", FormatDateTimeAsIMFFix(Value(last_modified)));
            }
            return response;
          } else {
            // Export requested via `?export`, dump the raw JSON record.
            return value;
          }
        } else {
          return ErrorResponse(
              ResourceNotFoundError(
                  "The resource with the requested key has found been.",
                  {{"key", field_type_dependent_t<PARTICULAR_FIELD>::FormatURLKey(url_key_value)}}),
              HTTPResponseCode.NotFound);
        }
      } else {
        // Collection view.
        if (!input.export_requested) {
          // Default, paginated, collection view.
          // `data` is an array of `AdvancedHypermediaRESTRecordResponse<brief_entry_t>`.
          using data_entry_t = AdvancedHypermediaRESTRecordResponse<brief_entry_t>;
          AdvancedHypermediaRESTContainerResponse<data_entry_t> response;
          response.url_directory =
              input.restful_url_prefix + '/' + kRESTfulDataURLComponent + '/' + input.field_name;
          const auto GenPageURL = [&](uint64_t i, uint64_t n) {
            return input.restful_url_prefix + '/' + kRESTfulDataURLComponent + '/' + input.field_name + "?i=" +
                   current::ToString(i) + "&n=" + current::ToString(n);
          };
          // Poor man's pagination.
          uint64_t i = 0;
          bool has_previous_page = false;
          bool has_next_page = false;
          for (const auto& element : input.field) {
            if (i >= query_i && i < query_i + query_n) {
              response.data.push_back(FormatAsAdvancedHypermediaRecord<brief_entry_t>(element, input, false));
            } else if (i < query_i) {
              has_previous_page = true;
            } else if (i >= query_i + query_n) {
              has_next_page = true;
              break;
            }
            ++i;
          }
          if (query_i > i) {
            query_i = i;
          }
          response.url = GenPageURL(query_i, query_n);
          response.i = query_i;
          response.n = std::min(query_n, i - query_i);
          response.total = i;
          if (has_previous_page) {
            response.url_previous_page = GenPageURL(query_i >= query_n ? query_i - query_n : 0, query_n);
          }
          if (has_next_page) {
            response.url_next_page =
                GenPageURL(query_i + query_n * 2 > i ? i - query_n : query_i + query_n, query_n);
          }
          return Response(response, HTTPResponseCode.OK);
        } else {
#ifndef CURRENT_ALLOW_STORAGE_EXPORT_FROM_MASTER
          // Export requested via `?export`, dump all the records.
          // Slow. Only available off the followers.
          if (input.role != StorageRole::Follower) {
            return ErrorResponse(
                HypermediaRESTError("NotFollowerMode", "Can only request full export from a Follower storage."),
                HTTPResponseCode.Forbidden);
          } else
#endif  // CURRENT_ALLOW_STORAGE_EXPORT_FROM_MASTER
          {
            // Sadly, the `Response` must be returned.
            // Have to create it in memory for now. -- D.K.
            // TODO(dkorolev): Migrate to a better way.
            std::ostringstream result;
            for (const auto& element : input.field) {
              result << JSON<JSONFormat::Minimalistic>(element) << '\n';
            }
            return result.str();
          }
        }
      }
    }

    template <class INPUT, typename FIELD_SEMANTICS, typename KEY_COMPLETENESS>
    Response RunForFullOrPartialKey(const INPUT& input,
                                    KEY_COMPLETENESS,
                                    FIELD_SEMANTICS,
                                    semantics::key_completeness::MatrixHalfKey) const {
      return SUPER_GET_HANDLER::RunForFullOrPartialKey(
          input, KEY_COMPLETENESS(), FIELD_SEMANTICS(), semantics::key_completeness::MatrixHalfKey());
    }

    template <class INPUT>
    Response Run(const INPUT& input) const {
      return RunForFullOrPartialKey(input,
                                    typename INPUT::key_completeness_t(),
                                    typename INPUT::field_t::semantics_t(),
                                    typename INPUT::key_completeness_t::completeness_family_t());
    }
  };
};

}  // namespace rest
}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_REST_ADVANCED_HYPERMEDIA_H
