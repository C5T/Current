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

#ifndef CURRENT_STORAGE_REST_HYPERMEDIA_H
#define CURRENT_STORAGE_REST_HYPERMEDIA_H

#include "../storage.h"

#include "../../TypeSystem/struct.h"
#include "../../Blocks/HTTP/api.h"

CURRENT_STRUCT(HypermediaRESTError) {
  CURRENT_FIELD(error, std::string);
  CURRENT_CONSTRUCTOR(HypermediaRESTError)(const std::string& error) : error(error) {}
};

CURRENT_STRUCT(HypermediaRESTParseJSONError, HypermediaRESTError) {
  CURRENT_FIELD(json_details, std::string);
  CURRENT_CONSTRUCTOR(HypermediaRESTParseJSONError)(const std::string& error, const std::string& json_details)
      : SUPER(error), json_details(json_details) {}
};

namespace current {
namespace storage {
namespace rest {

struct Hypermedia {
  template <class HTTP_VERB, typename ALL_FIELDS, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTful;

  template <typename ALL_FIELDS, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTful<GET, ALL_FIELDS, PARTICULAR_FIELD, ENTRY, KEY> {
    template <typename F>
    void Enter(Request request, F&& next) {
      if (request.url_path_args.size() != 1) {
        request(HypermediaRESTError("Need resource key in the URL."), HTTPResponseCode.BadRequest);
      } else {
        next(std::move(request));
      }
    }
    template <class INPUT>
    Response Run(const INPUT& input) const {
      const ImmutableOptional<ENTRY> result = input.field[input.key];
      if (Exists(result)) {
        return Value(result);
      } else {
        return Response(HypermediaRESTError("Resource not found."), HTTPResponseCode.NotFound);
      }
    }
  };

  template <typename ALL_FIELDS, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTful<POST, ALL_FIELDS, PARTICULAR_FIELD, ENTRY, KEY> {
    template <typename F>
    void Enter(Request request, F&& next) {
      if (!request.url_path_args.empty()) {
        request(HypermediaRESTError("Should not have resource key in the URL."), HTTPResponseCode.BadRequest);
      } else {
        next(std::move(request));
      }
    }
    template <class INPUT>
    Response Run(const INPUT& input) const {
      input.field.Add(input.entry);
      // TODO(dkorolev): Return a JSON with a resource here.
      return Response("Added.\n", HTTPResponseCode.NoContent);
    }
    static Response ErrorBadJSON(const std::string& error_message) {
      return Response(HypermediaRESTParseJSONError("Invalid JSON in request body.", error_message),
                      HTTPResponseCode.BadRequest);
    }
  };

  template <typename ALL_FIELDS, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTful<DELETE, ALL_FIELDS, PARTICULAR_FIELD, ENTRY, KEY> {
    template <typename F>
    void Enter(Request request, F&& next) {
      if (request.url_path_args.size() != 1) {
        request(HypermediaRESTError("Need resource key in the URL."), HTTPResponseCode.BadRequest);
      } else {
        next(std::move(request));
      }
    }
    template <class INPUT>
    Response Run(const INPUT& input) const {
      input.field.Erase(input.key);
      // TODO(dkorolev): Return a JSON with something more useful here.
      return Response("Deleted.\n", HTTPResponseCode.NoContent);
    }
  };

  static Response ErrorMethodNotAllowed() {
    return Response(HypermediaRESTError("Method not allowed."), HTTPResponseCode.MethodNotAllowed);
  }
};

}  // namespace rest
}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_REST_HYPERMEDIA_H
