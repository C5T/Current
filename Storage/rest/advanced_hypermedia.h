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
  AdvancedHypermediaRESTRecordResponse<T> response;
  const std::string key_as_string = current::ToString(
      PerStorageFieldType<current::decay<decltype(input.field)>>::ExtractOrComposeKey(record));
  if (set_success) {
    response.success = true;
  }
  response.url_directory = input.restful_url_prefix + "/data/" + input.field_name;
  response.url = response.url_directory + '/' + key_as_string;
  response.url_full = response.url;
  response.url_brief = response.url + "?fields=brief";
  response.data = static_cast<const T&>(record);
  return response;
}

struct AdvancedHypermedia : Hypermedia {
  using SUPER = Hypermedia;

  template <class HTTP_VERB, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTful : SUPER::RESTful<HTTP_VERB, PARTICULAR_FIELD, ENTRY, KEY> {};

  template <typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTful<GET, PARTICULAR_FIELD, ENTRY, KEY> {
    using brief_entry_t = sfinae::BRIEF_OF_T<ENTRY>;

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
      WithOptionalKeyFromURL(std::move(request), std::forward<F>(next));
    }

    template <class INPUT>
    Response Run(const INPUT& input) const {
      if (!input.url_key.empty()) {
        // Single record view.
        const ImmutableOptional<ENTRY> result = input.field[current::FromString<KEY>(input.url_key)];
        if (Exists(result)) {
          const auto& value = Value(result);
          return (brief ? Response(FormatAsAdvancedHypermediaRecord<brief_entry_t>(value, input),
                                   HTTPResponseCode.OK)
                        : Response(FormatAsAdvancedHypermediaRecord<ENTRY>(value, input), HTTPResponseCode.OK));
        } else {
          return ErrorResponse(
              ResourceNotFoundError("Resource with requested key not found.", {{"key", input.url_key}}),
              HTTPResponseCode.NotFound);
        }
      } else {
        // Collection view. `data` is an array of `AdvancedHypermediaRESTRecordResponse<brief_entry_t>`.
        using data_entry_t = AdvancedHypermediaRESTRecordResponse<brief_entry_t>;
        AdvancedHypermediaRESTContainerResponse<data_entry_t> response;
        response.url_directory = input.restful_url_prefix + "/data/" + input.field_name;
        const auto GenPageURL = [&](uint64_t i, uint64_t n) {
          return input.restful_url_prefix + "/data/" + input.field_name + "?i=" + current::ToString(i) + "&n=" +
                 current::ToString(n);
        };
        // Poor man's pagination.
        uint64_t i = 0;
        bool has_previous_page = false;
        bool has_next_page = false;
        for (const auto& element : PerStorageFieldType<PARTICULAR_FIELD>::Iterate(input.field)) {
          if (i >= query_i && i < query_i + query_n) {
            response.data.push_back(FormatAsAdvancedHypermediaRecord<brief_entry_t>(element, input, false));
          } else if (i < query_i) {
            has_previous_page = true;
          } else if (i >= query_i + query_n) {
            has_next_page = true;
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
      }
    }
  };
};

}  // namespace rest
}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_REST_ADVANCED_HYPERMEDIA_H
