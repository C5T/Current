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

#include "sfinae.h"

#include "../storage.h"

#include "../../TypeSystem/struct.h"
#include "../../Blocks/HTTP/api.h"

namespace current {
namespace storage {
namespace rest {

CURRENT_STRUCT(HypermediaRESTTopLevel) {
  CURRENT_FIELD(url, std::string);
  CURRENT_FIELD(url_status, std::string);
  CURRENT_FIELD(url_data, (std::map<std::string, std::string>));
  CURRENT_FIELD(up, bool);

  CURRENT_CONSTRUCTOR(HypermediaRESTTopLevel)(const std::string& url_prefix = "", bool up = false)
      : url(url_prefix), url_status(url_prefix + "/status"), up(up) {}
};

CURRENT_STRUCT(HypermediaRESTStatus) {
  CURRENT_FIELD(up, bool);

  CURRENT_CONSTRUCTOR(HypermediaRESTStatus)(bool up = false) : up(up) {}
};

CURRENT_STRUCT(HypermediaRESTError) {
  using T_DETAILS = std::map<std::string, std::string>;
  CURRENT_FIELD(name, std::string);
  CURRENT_FIELD(description, std::string);
  CURRENT_FIELD(details, Optional<T_DETAILS>);

  CURRENT_CONSTRUCTOR(HypermediaRESTError)(const std::string& name, const std::string& description)
      : name(name), description(description) {}
  CURRENT_CONSTRUCTOR(HypermediaRESTError)(
      const std::string& name, const std::string& description, const T_DETAILS& details)
      : name(name), description(description), details(details) {}
};

CURRENT_STRUCT(HypermediaRESTGenericResponse) {
  CURRENT_FIELD(success, bool, true);
  CURRENT_FIELD(message, Optional<std::string>);
  CURRENT_FIELD(error, Optional<HypermediaRESTError>);

  CURRENT_DEFAULT_CONSTRUCTOR(HypermediaRESTGenericResponse) {}
  CURRENT_CONSTRUCTOR(HypermediaRESTGenericResponse)(bool success) : success(success) {}
  CURRENT_CONSTRUCTOR(HypermediaRESTGenericResponse)(bool success, const std::string& message)
      : success(success), message(message) {}
  CURRENT_CONSTRUCTOR(HypermediaRESTGenericResponse)(
      bool success, const std::string& message, const HypermediaRESTError& error)
      : success(success), message(message), error(error) {}
  CURRENT_CONSTRUCTOR(HypermediaRESTGenericResponse)(bool success, const HypermediaRESTError& error)
      : success(success), error(error) {}
};

CURRENT_STRUCT_T(HypermediaRESTRecordResponse) {
  CURRENT_FIELD(success, bool, true);  // No nested template structs yet :(
  CURRENT_FIELD(url, std::string);
  CURRENT_FIELD(data, T);

  CURRENT_CONSTRUCTOR_T(HypermediaRESTRecordResponse)(const std::string& url, const T& data)
      : url(url), data(data) {}
  CURRENT_CONSTRUCTOR_T(HypermediaRESTRecordResponse)(const std::string& url, T&& data)
      : url(url), data(std::move(data)) {}
};

CURRENT_STRUCT(HypermediaRESTContainerResponse, HypermediaRESTGenericResponse) {
  CURRENT_FIELD(url, std::string);
  CURRENT_FIELD(data, std::vector<std::string>);

  CURRENT_DEFAULT_CONSTRUCTOR(HypermediaRESTContainerResponse) : SUPER(true) {}
  CURRENT_CONSTRUCTOR(HypermediaRESTContainerResponse)(const std::string& url,
                                                       const std::vector<std::string>& data)
      : SUPER(true), url(url), data(data) {}
  CURRENT_CONSTRUCTOR(HypermediaRESTContainerResponse)(const std::string& url, std::vector<std::string>&& data)
      : SUPER(true), url(url), data(std::move(data)) {}
};

CURRENT_STRUCT(HypermediaRESTResourceUpdateResponse, HypermediaRESTGenericResponse) {
  CURRENT_FIELD(resource_url, Optional<std::string>);

  CURRENT_CONSTRUCTOR(HypermediaRESTResourceUpdateResponse)(bool success) : SUPER(success) {}
  CURRENT_CONSTRUCTOR(HypermediaRESTResourceUpdateResponse)(bool success, const std::string& message)
      : SUPER(success, message) {}
  CURRENT_CONSTRUCTOR(HypermediaRESTResourceUpdateResponse)(
      bool success, const std::string& message, const std::string& resource_url)
      : SUPER(success, message), resource_url(resource_url) {}
  CURRENT_CONSTRUCTOR(HypermediaRESTResourceUpdateResponse)(
      bool success, const std::string& message, const HypermediaRESTError& error)
      : SUPER(success, message, error) {}
};

inline HypermediaRESTGenericResponse ErrorResponseObject(const std::string& message,
                                                         const HypermediaRESTError& error) {
  return HypermediaRESTGenericResponse(false, message, error);
}

inline HypermediaRESTGenericResponse ErrorResponseObject(const HypermediaRESTError& error) {
  return HypermediaRESTGenericResponse(false, error);
}

inline Response ErrorResponse(const HypermediaRESTError& error_object, net::HTTPResponseCodeValue code) {
  return Response(ErrorResponseObject(error_object), code);
}

inline HypermediaRESTError MethodNotAllowedError(const std::string& description,
                                                 const std::string& requested_method) {
  return HypermediaRESTError("MethodNotAllowed", description, {{"requested_method", requested_method}});
}

inline HypermediaRESTError ParseJSONError(const std::string& description, const std::string& error_details) {
  return HypermediaRESTError("ParseJSONError", description, {{"error_details", error_details}});
};

inline HypermediaRESTError RequiredKeyIsMissingError(const std::string& description) {
  return HypermediaRESTError("RequiredKeyIsMissing", description);
}

inline HypermediaRESTError InvalidKeyError(const std::string& description) {
  return HypermediaRESTError("InvalidKey", description);
}

inline HypermediaRESTError InvalidKeyError(const std::string& description,
                                           const std::map<std::string, std::string>& details) {
  return HypermediaRESTError("InvalidKey", description, details);
}

inline HypermediaRESTError ResourceNotFoundError(const std::string& description,
                                                 const std::map<std::string, std::string>& details) {
  return HypermediaRESTError("ResourceNotFound", description, details);
}

inline HypermediaRESTError ResourceAlreadyExistsError(const std::string& description,
                                                      const std::map<std::string, std::string>& details) {
  return HypermediaRESTError("ResourceAlreadyExists", description, details);
}

struct Hypermedia {
  template <class HTTP_VERB, typename ALL_FIELDS, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTful;

  static void RegisterTopLevel(HTTPRoutesScope& scope,
                               const std::vector<std::string>& fields,
                               int port,
                               const std::string& route_prefix,
                               const std::string& restful_url_prefix,
                               const std::string& data_url_component,
                               std::atomic_bool& up_status) {
    scope +=
        HTTP(port).Register(route_prefix,
                            [fields, restful_url_prefix, data_url_component, &up_status](Request request) {
                              const bool up = up_status;
                              HypermediaRESTTopLevel response(restful_url_prefix, up);
                              for (const auto& f : fields) {
                                response.url_data[f] = restful_url_prefix + '/' + data_url_component + '/' + f;
                              }
                              request(response, up ? HTTPResponseCode.OK : HTTPResponseCode.ServiceUnavailable);
                            });
    scope += HTTP(port).Register(route_prefix == "/" ? "/status" : route_prefix + "/status",
                                 [restful_url_prefix, &up_status](Request request) {
                                   const bool up = up_status;
                                   request(HypermediaRESTStatus(up),
                                           up ? HTTPResponseCode.OK : HTTPResponseCode.ServiceUnavailable);
                                 });
  }

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
        [](Request request) {
          request(ErrorResponseObject(RequiredKeyIsMissingError("Need resource key in the URL.")),
                  HTTPResponseCode.BadRequest);
        });
  }

  template <typename F>
  static void WithOptionalKeyFromURL(Request request, F&& next) {
    WithOrWithoutKeyFromURL(
        std::move(request), next, [&next](Request request) { next(std::move(request), ""); });
  }

  template <typename ALL_FIELDS, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTful<GET, ALL_FIELDS, PARTICULAR_FIELD, ENTRY, KEY> {
    template <typename F>
    void Enter(Request request, F&& next) {
      WithOptionalKeyFromURL(std::move(request), std::forward<F>(next));
    }
    template <class INPUT>
    Response Run(const INPUT& input) const {
      if (!input.url_key.empty()) {
        const ImmutableOptional<ENTRY> result = input.field[current::FromString<KEY>(input.url_key)];
        if (Exists(result)) {
          const std::string url = input.restful_url_prefix + '/' + input.data_url_component + '/' +
                                  input.field_name + '/' + input.url_key;
          return HypermediaRESTRecordResponse<ENTRY>(url, Value(result));
        } else {
          return ErrorResponse(
              ResourceNotFoundError("The requested resource not found.", {{"key", input.url_key}}),
              HTTPResponseCode.NotFound);
        }
      } else {
        HypermediaRESTContainerResponse response;
        response.url = input.restful_url_prefix + '/' + input.data_url_component + '/' + input.field_name;
        for (const auto& element : PerStorageFieldType<PARTICULAR_FIELD>::Iterate(input.field)) {
          response.data.emplace_back(
              response.url + '/' +
              current::ToString(PerStorageFieldType<PARTICULAR_FIELD>::ExtractOrComposeKey(element)));
        }
        return Response(response);
      }
    }
  };

  template <typename ALL_FIELDS, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTful<POST, ALL_FIELDS, PARTICULAR_FIELD, ENTRY, KEY> {
    template <typename F>
    void Enter(Request request, F&& next) {
      if (!request.url_path_args.empty()) {
        request(ErrorResponseObject(InvalidKeyError("Should not have resource key in the URL.")),
                HTTPResponseCode.BadRequest);
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
      return Hypermedia::ErrorMethodNotAllowed("POST");
    }
    template <class INPUT, bool B>
    ENABLE_IF<B, Response> RunImpl(const INPUT& input) const {
      input.entry.InitializeOwnKey();
      const auto entry_key = PerStorageFieldType<PARTICULAR_FIELD>::ExtractOrComposeKey(input.entry);
      const std::string key = current::ToString(entry_key);
      if (!Exists(input.field[entry_key])) {
        input.field.Add(input.entry);
        const std::string url =
            input.restful_url_prefix + '/' + input.data_url_component + '/' + input.field_name + '/' + key;
        return Response(HypermediaRESTResourceUpdateResponse(true, "Resource created.", url),
                        HTTPResponseCode.Created);
      } else {
        return Response(HypermediaRESTResourceUpdateResponse(
                            false,
                            "Resource creation failed.",
                            {ResourceAlreadyExistsError("The resource already exists", {{"key", key}})}),
                        HTTPResponseCode.Conflict);
      }
    }
    static Response ErrorBadJSON(const std::string& error_message) {
      return ErrorResponse(ParseJSONError("Invalid JSON in request body.", error_message),
                           HTTPResponseCode.BadRequest);
    }
  };

  template <typename ALL_FIELDS, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTful<PUT, ALL_FIELDS, PARTICULAR_FIELD, ENTRY, KEY> {
    template <typename F>
    void Enter(Request request, F&& next) {
      WithKeyFromURL(std::move(request), std::forward<F>(next));
    }
    template <class INPUT>
    Response Run(const INPUT& input) const {
      const std::string url_key = current::ToString(input.url_key);
      if (input.entry_key == input.url_key) {
        const bool exists = Exists(input.field[input.entry_key]);
        input.field.Add(input.entry);
        const std::string url =
            input.restful_url_prefix + '/' + input.data_url_component + '/' + input.field_name + '/' + url_key;
        HypermediaRESTResourceUpdateResponse response(true);
        if (exists) {
          response.message = "Resource updated.";
          return Response(response, HTTPResponseCode.OK);
        } else {
          response.message = "Resource created.";
          return Response(response, HTTPResponseCode.Created);
        }
      } else {
        const std::string object_key = current::ToString(input.entry_key);
        return ErrorResponse(InvalidKeyError("Object key doesn't match URL key.",
                                             {{"object_key", object_key}, {"url_key", url_key}}),
                             HTTPResponseCode.BadRequest);
      }
    }
    static Response ErrorBadJSON(const std::string& error_message) {
      return ErrorResponse(ParseJSONError("Invalid JSON in request body.", error_message),
                           HTTPResponseCode.BadRequest);
    }
  };
  template <typename ALL_FIELDS, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTful<DELETE, ALL_FIELDS, PARTICULAR_FIELD, ENTRY, KEY> {
    template <typename F>
    void Enter(Request request, F&& next) {
      WithKeyFromURL(std::move(request), std::forward<F>(next));
    }
    template <class INPUT>
    Response Run(const INPUT& input) const {
      std::string message;
      if (Exists(input.field[input.key])) {
        message = "Resource deleted.";
      } else {
        message = "Resource didn't exist.";
      }
      input.field.Erase(input.key);
      return Response(HypermediaRESTResourceUpdateResponse(true, message), HTTPResponseCode.OK);
    }
  };

  static Response ErrorMethodNotAllowed(const std::string& method) {
    return ErrorResponse(MethodNotAllowedError("Supported methods: GET, PUT, POST, DELETE.", method),
                         HTTPResponseCode.MethodNotAllowed);
  }
};

}  // namespace rest
}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_REST_HYPERMEDIA_H
