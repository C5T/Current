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

CURRENT_STRUCT(AdvancedHypermediaRESTRecordResponse) {
  CURRENT_FIELD(url, std::string);
  CURRENT_FIELD(url_full, std::string);
  CURRENT_FIELD(url_brief, std::string);
  CURRENT_FIELD(url_directory, std::string);
  // The `"data":{...}` blob is injected as a raw string for now. -- D.K.
};

CURRENT_STRUCT(AdvancedHypermediaRESTContainerResponse) {
  CURRENT_FIELD(url, std::string);
  // The `"data":[...]` blob is injected as a raw string for now. -- D.K.
};

namespace current {
namespace storage {
namespace rest {

static std::string json_content_type = current::net::HTTPServerConnection::DefaultJSONContentType();

struct AdvancedHypermedia : Hypermedia {
  using SUPER = Hypermedia;

  template <class HTTP_VERB, typename ALL_FIELDS, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTful : SUPER::RESTful<HTTP_VERB, ALL_FIELDS, PARTICULAR_FIELD, ENTRY, KEY> {};

  template <typename ALL_FIELDS, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTful<GET, ALL_FIELDS, PARTICULAR_FIELD, ENTRY, KEY> {
    using T_BRIEF_ENTRY = typename ENTRY::T_BRIEF;

    bool brief = false;

    template <typename F>
    void Enter(Request request, F&& next) {
      brief = (request.url.query["fields"] == "brief");
      WithOptionalKeyFromURL(std::move(request), std::forward<F>(next));
    }

    template <class INPUT>
    Response Run(const INPUT& input) const {
      if (!input.url_key.empty()) {
        // Single record view.
        const ImmutableOptional<ENTRY> result = input.field[FromString<KEY>(input.url_key)];
        if (Exists(result)) {
          const auto& value = Value(result);
          // Format the resulting data, in full or brief format.
          const std::string data = [this, &value]() {
            if (!brief) {
              return JSON(value);
            } else {
              return JSON(static_cast<const T_BRIEF_ENTRY&>(value));
            }
          }();

          // And wrap it into a hypermedia response.
          const std::string key_as_string = ToString(sfinae::GetKey(value));
          AdvancedHypermediaRESTRecordResponse response_builder;
          response_builder.url_directory = input.restful_url_prefix + '/' + input.field_name;
          response_builder.url = response_builder.url_directory + '/' + key_as_string;
          response_builder.url_full = response_builder.url;
          response_builder.url_brief = response_builder.url + "?fields=brief";
          const std::string response = JSON(response_builder);
          return Response(response.substr(0, response.length() - 1) + ",\"data\":" + data + '}',
                          HTTPResponseCode.OK,
                          json_content_type);
        } else {
          return Response(HypermediaRESTError("Resource not found."), HTTPResponseCode.NotFound);
        }
      } else {
        // Collection view.
        AdvancedHypermediaRESTContainerResponse response_builder;
        response_builder.url = input.restful_url_prefix + '/' + input.field_name;
        bool first = true;
        std::ostringstream os;
        for (const auto& element : input.field) {
          if (first) {
            first = false;
          } else {
            os << ',';
          }
          os << JSON(static_cast<const T_BRIEF_ENTRY&>(element));
        }
        const std::string response = JSON(response_builder);
        return Response(response.substr(0, response.length() - 1) + ",\"data\":[" + os.str() + "]}",
                        HTTPResponseCode.OK,
                        json_content_type);
      }
    }
  };
};

}  // namespace rest
}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_REST_ADVANCED_HYPERMEDIA_H
