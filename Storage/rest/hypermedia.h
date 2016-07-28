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

#include "basic.h"
#include "sfinae.h"

#include "../api_types.h"
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
  using details_t = std::map<std::string, std::string>;
  CURRENT_FIELD(name, std::string, "");
  CURRENT_FIELD(message, std::string, "");
  CURRENT_FIELD(details, Optional<details_t>);

  CURRENT_DEFAULT_CONSTRUCTOR(HypermediaRESTError) {}
  CURRENT_CONSTRUCTOR(HypermediaRESTError)(const std::string& name, const std::string& message)
      : name(name), message(message) {}
  CURRENT_CONSTRUCTOR(HypermediaRESTError)(
      const std::string& name, const std::string& message, const details_t& details)
      : name(name), message(message), details(details) {}
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
  CURRENT_FIELD(url, std::string, "");
  CURRENT_FIELD(data, T);

  CURRENT_DEFAULT_CONSTRUCTOR_T(HypermediaRESTRecordResponse) {}
  CURRENT_CONSTRUCTOR_T(HypermediaRESTRecordResponse)(const std::string& url, const T& data)
      : url(url), data(data) {}
  CURRENT_CONSTRUCTOR_T(HypermediaRESTRecordResponse)(const std::string& url, T&& data)
      : url(url), data(std::move(data)) {}
};

CURRENT_STRUCT(HypermediaRESTContainerResponse, HypermediaRESTGenericResponse) {
  CURRENT_FIELD(url, std::string, "");
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

inline HypermediaRESTError MethodNotAllowedError(const std::string& message,
                                                 const std::string& requested_method) {
  return HypermediaRESTError("MethodNotAllowed", message, {{"requested_method", requested_method}});
}

inline HypermediaRESTError InvalidHeaderError(const std::string& message,
                                              const std::string& header,
                                              const std::string& value) {
  return HypermediaRESTError("InvalidHeader", message, {{"header", header}, {"header_value", value}});
}

inline HypermediaRESTError ParseJSONError(const std::string& message, const std::string& error_details) {
  return HypermediaRESTError("ParseJSONError", message, {{"error_details", error_details}});
};

inline HypermediaRESTError RequiredKeyIsMissingError(const std::string& message) {
  return HypermediaRESTError("RequiredKeyIsMissing", message);
}

inline HypermediaRESTError InvalidKeyError(const std::string& message) {
  return HypermediaRESTError("InvalidKey", message);
}

inline HypermediaRESTError InvalidKeyError(const std::string& message,
                                           const std::map<std::string, std::string>& details) {
  return HypermediaRESTError("InvalidKey", message, details);
}

inline HypermediaRESTError ResourceNotFoundError(const std::string& message,
                                                 const std::map<std::string, std::string>& details) {
  return HypermediaRESTError("ResourceNotFound", message, details);
}

inline HypermediaRESTError ResourceAlreadyExistsError(const std::string& message,
                                                      const std::map<std::string, std::string>& details) {
  return HypermediaRESTError("ResourceAlreadyExists", message, details);
}

inline HypermediaRESTError ResourceWasModifiedError(const std::string& message,
                                                    std::chrono::microseconds requested,
                                                    std::chrono::microseconds last_modified) {
  return HypermediaRESTError("ResourceWasModifiedError",
                             message,
                             {{"requested_date", FormatDateTimeAsIMFFix(requested)},
                              {"resource_last_modified_date", FormatDateTimeAsIMFFix(last_modified)}});
}

struct Hypermedia {
  template <class HTTP_VERB, typename OPERATION, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTfulDataHandlerGenerator;

  template <class INPUT>
  static void RegisterTopLevel(const INPUT& input) {
    input.scope +=
        HTTP(input.port)
            .Register(input.route_prefix,
                      [input](Request request) {
                        const bool up = input.up_status;
                        HypermediaRESTTopLevel response(input.restful_url_prefix, up);
                        for (const auto& f : input.field_names) {
                          response.url_data[f] =
                              input.restful_url_prefix + '/' + kRESTfulDataURLComponent + '/' + f;
                        }
                        request(response, up ? HTTPResponseCode.OK : HTTPResponseCode.ServiceUnavailable);
                      });
    input.scope += HTTP(input.port)
                       .Register(input.route_prefix == "/" ? "/status" : input.route_prefix + "/status",
                                 [input](Request request) {
                                   const bool up = input.up_status;
                                   request(HypermediaRESTStatus(up),
                                           up ? HTTPResponseCode.OK : HTTPResponseCode.ServiceUnavailable);
                                 });
  }

  // Returns `false` on error.
  static bool ExtractIfUnmodifiedSinceOrRespondWithError(Request& request,
                                                         Optional<std::chrono::microseconds>& destination) {
    if (request.headers.Has("If-Unmodified-Since")) {
      const auto& header_value = request.headers.Get("If-Unmodified-Since");
      try {
        destination = net::http::ParseHTTPDate(header_value);
      } catch (const current::net::http::InvalidHTTPDateException&) {
        request(ErrorResponseObject(
                    InvalidHeaderError("Unparsable datetime value.", "If-Unmodified-Since", header_value)),
                HTTPResponseCode.BadRequest);
        return false;
      }
    } else {
      destination = nullptr;
    }
    return true;
  }

  template <typename OPERATION, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTfulDataHandlerGenerator<GET, OPERATION, PARTICULAR_FIELD, ENTRY, KEY> {
    template <typename F>
    void EnterByKeyCompletenessFamily(Request request,
                                      semantics::key_completeness::FullKey,
                                      semantics::key_completeness::DictionaryOrMatrixCompleteKey,
                                      F&& next) {
      field_type_dependent_t<PARTICULAR_FIELD>::CallWithOptionalKeyFromURL(std::move(request),
                                                                           std::forward<F>(next));
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

    template <class INPUT, typename FIELD_SEMANTICS>
    Response RunForFullOrPartialKey(const INPUT& input,
                                    semantics::key_completeness::FullKey,
                                    FIELD_SEMANTICS,
                                    semantics::key_completeness::DictionaryOrMatrixCompleteKey) const {
      if (Exists(input.get_url_key)) {
        const auto url_key_value = Value(input.get_url_key);
        const auto key = field_type_dependent_t<PARTICULAR_FIELD>::template ParseURLKey<KEY>(url_key_value);
        const ImmutableOptional<ENTRY> result = input.field[key];
        if (Exists(result)) {
          const std::string url = input.restful_url_prefix + '/' + kRESTfulDataURLComponent + '/' +
                                  input.field_name + '/' +
                                  field_type_dependent_t<PARTICULAR_FIELD>::FormatURLKey(url_key_value);
          auto response = Response(HypermediaRESTRecordResponse<ENTRY>(url, Value(result)));
          const auto last_modified = input.field.LastModified(key);
          if (Exists(last_modified)) {
            response.SetHeader("Last-Modified", FormatDateTimeAsIMFFix(Value(last_modified)));
          }
          return response;
        } else {
          return ErrorResponse(
              ResourceNotFoundError(
                  "The requested resource not found.",
                  {{"key", field_type_dependent_t<PARTICULAR_FIELD>::FormatURLKey(url_key_value)}}),
              HTTPResponseCode.NotFound);
        }
      } else {
        HypermediaRESTContainerResponse response;
        response.url = input.restful_url_prefix + '/' + kRESTfulDataURLComponent + '/' + input.field_name;
        for (const auto& element : input.field) {
          response.data.emplace_back(
              response.url + '/' + field_type_dependent_t<PARTICULAR_FIELD>::ComposeURLKey(
                                       field_type_dependent_t<PARTICULAR_FIELD>::ExtractOrComposeKey(element)));
        }
        return Response(response);
      }
    }

    template <class INPUT, typename FIELD_SEMANTICS, typename KEY_COMPLETENESS>
    Response RunForFullOrPartialKey(const INPUT& input,
                                    KEY_COMPLETENESS,
                                    FIELD_SEMANTICS,
                                    semantics::key_completeness::MatrixHalfKey) const {
      if (Exists(input.rowcol_get_url_key)) {
        const auto row_or_col_key = current::FromString<
            typename MatrixContainerProxy<KEY_COMPLETENESS>::template sfinae_based_outer_key_t<ENTRY>>(
            Value(input.rowcol_get_url_key));
        const auto iterable =
            GenericMatrixIterator<KEY_COMPLETENESS, FIELD_SEMANTICS>::RowOrCol(input.field, row_or_col_key);
        if (!iterable.Empty()) {
          HypermediaRESTContainerResponse response;
          response.url = input.restful_url_prefix + '/' + kRESTfulDataURLComponent + '/' + input.field_name;
          for (const auto& element : iterable) {
            response.data.emplace_back(
                response.url + '/' +
                field_type_dependent_t<PARTICULAR_FIELD>::ComposeURLKey(
                    field_type_dependent_t<PARTICULAR_FIELD>::ExtractOrComposeKey(element)));
          }
          return Response(response);
        } else {
          return ErrorResponse(ResourceNotFoundError("The requested key has was not found.",
                                                     {{"key", Value(input.rowcol_get_url_key)}}),
                               HTTPResponseCode.NotFound);
        }
      } else {
        HypermediaRESTContainerResponse response;
        response.url = input.restful_url_prefix + '/' + kRESTfulDataURLComponent + '/' + input.field_name +
                       '.' + MatrixContainerProxy<KEY_COMPLETENESS>::PartialKeySuffix();
        const auto iterable = GenericMatrixIterator<KEY_COMPLETENESS, FIELD_SEMANTICS>::RowsOrCols(input.field);
        for (auto iterator = iterable.begin(); iterator != iterable.end(); ++iterator) {
          response.data.emplace_back(response.url + '/' + current::ToString(iterator.key()));
        }
        return Response(response);
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
  struct RESTfulDataHandlerGenerator<POST, OPERATION, PARTICULAR_FIELD, ENTRY, KEY> {
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
      const auto entry_key = field_type_dependent_t<PARTICULAR_FIELD>::ExtractOrComposeKey(input.entry);
      const std::string key = field_type_dependent_t<PARTICULAR_FIELD>::ComposeURLKey(entry_key);
      if (!Exists(input.field[entry_key])) {
        input.field.Add(input.entry);
        const std::string url =
            input.restful_url_prefix + '/' + kRESTfulDataURLComponent + '/' + input.field_name + '/' + key;
        auto response = Response(HypermediaRESTResourceUpdateResponse(true, "Resource created.", url),
                                 HTTPResponseCode.Created);
        const auto last_modified = input.field.LastModified(entry_key);
        if (Exists(last_modified)) {
          response.SetHeader("Last-Modified", FormatDateTimeAsIMFFix(Value(last_modified)));
        }
        return response;
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

  template <typename OPERATION, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTfulDataHandlerGenerator<PUT, OPERATION, PARTICULAR_FIELD, ENTRY, KEY> {
    Optional<std::chrono::microseconds> if_unmodified_since;

    template <typename F>
    void Enter(Request request, F&& next) {
      if (ExtractIfUnmodifiedSinceOrRespondWithError(request, if_unmodified_since)) {
        field_type_dependent_t<PARTICULAR_FIELD>::CallWithKeyFromURL(std::move(request), std::forward<F>(next));
      }
    }
    template <class INPUT>
    Response Run(const INPUT& input) const {
      const std::string url_key = field_type_dependent_t<PARTICULAR_FIELD>::ComposeURLKey(input.put_key);
      if (input.entry_key == input.put_key) {
        const bool exists = Exists(input.field[input.entry_key]);
        if (exists && Exists(if_unmodified_since)) {
          const auto last_modified = input.field.LastModified(input.entry_key);
          if (Exists(last_modified) && Value(last_modified).count() > Value(if_unmodified_since).count()) {
            return ErrorResponse(
                ResourceWasModifiedError("Resource can not be updated as it has been modified in the meantime.",
                                         Value(if_unmodified_since),
                                         Value(last_modified)),
                HTTPResponseCode.PreconditionFailed);
          }
        }
        input.field.Add(input.entry);
        const std::string url =
            input.restful_url_prefix + '/' + kRESTfulDataURLComponent + '/' + input.field_name + '/' + url_key;
        Response response;
        HypermediaRESTResourceUpdateResponse hypermedia_response(true);
        hypermedia_response.resource_url = url;
        if (exists) {
          hypermedia_response.message = "Resource updated.";
          response = Response(hypermedia_response, HTTPResponseCode.OK);
        } else {
          hypermedia_response.message = "Resource created.";
          response = Response(hypermedia_response, HTTPResponseCode.Created);
        }
        const auto last_modified = input.field.LastModified(input.entry_key);
        if (Exists(last_modified)) {
          response.SetHeader("Last-Modified", FormatDateTimeAsIMFFix(Value(last_modified)));
        }
        return response;
      } else {
        const std::string key_as_url_string =
            field_type_dependent_t<PARTICULAR_FIELD>::ComposeURLKey(input.entry_key);
        return ErrorResponse(InvalidKeyError("Object key doesn't match URL key.",
                                             {{"object_key", key_as_url_string}, {"url_key", url_key}}),
                             HTTPResponseCode.BadRequest);
      }
    }
    static Response ErrorBadJSON(const std::string& error_message) {
      return ErrorResponse(ParseJSONError("Invalid JSON in request body.", error_message),
                           HTTPResponseCode.BadRequest);
    }
  };

  template <typename OPERATION, typename PARTICULAR_FIELD, typename ENTRY, typename KEY>
  struct RESTfulDataHandlerGenerator<DELETE, OPERATION, PARTICULAR_FIELD, ENTRY, KEY> {
    Optional<std::chrono::microseconds> if_unmodified_since;

    template <typename F>
    void Enter(Request request, F&& next) {
      if (ExtractIfUnmodifiedSinceOrRespondWithError(request, if_unmodified_since)) {
        field_type_dependent_t<PARTICULAR_FIELD>::CallWithKeyFromURL(std::move(request), std::forward<F>(next));
      }
    }
    template <class INPUT>
    Response Run(const INPUT& input) const {
      std::string message;
      bool existed = false;
      if (Exists(input.field[input.delete_key])) {
        existed = true;
        if (Exists(if_unmodified_since)) {
          const auto last_modified = input.field.LastModified(input.delete_key);
          if (Exists(last_modified) && Value(last_modified).count() > Value(if_unmodified_since).count()) {
            return ErrorResponse(
                ResourceWasModifiedError("Resource can not be deleted as it has been modified in the meantime.",
                                         Value(if_unmodified_since),
                                         Value(last_modified)),
                HTTPResponseCode.PreconditionFailed);
          }
        }
        message = "Resource deleted.";
      } else {
        message = "Resource didn't exist.";
      }
      input.field.Erase(input.delete_key);
      auto response = Response(HypermediaRESTResourceUpdateResponse(true, message), HTTPResponseCode.OK);
      if (existed) {
        const auto last_modified = input.field.LastModified(input.delete_key);
        if (Exists(last_modified)) {
          response.SetHeader("Last-Modified", FormatDateTimeAsIMFFix(Value(last_modified)));
        }
      }
      return response;
    }
  };

  template <typename STORAGE, typename ENTRY>
  using RESTfulSchemaHandlerGenerator = Basic::template RESTfulSchemaHandlerGenerator<STORAGE, ENTRY>;

  static Response ErrorMethodNotAllowed(const std::string& method) {
    return ErrorResponse(MethodNotAllowedError("Supported methods: GET, PUT, POST, DELETE.", method),
                         HTTPResponseCode.MethodNotAllowed);
  }
};

}  // namespace rest
}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_REST_HYPERMEDIA_H
