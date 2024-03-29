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

#ifndef CURRENT_STORAGE_REST_TYPES_H
#define CURRENT_STORAGE_REST_TYPES_H

#include "../../typesystem/struct.h"
#include "../../typesystem/optional.h"
#include "../../blocks/http/response.h"

namespace current {
namespace storage {
namespace rest {

namespace generic {

CURRENT_STRUCT(RESTTopLevel) {
  CURRENT_FIELD(url, std::string);
  CURRENT_FIELD(url_status, std::string);
  CURRENT_FIELD(url_data, (std::map<std::string, std::string>));
  CURRENT_FIELD(up, bool);

  CURRENT_CONSTRUCTOR(RESTTopLevel)
  (const std::string& url_prefix = "", bool up = false) : url(url_prefix), url_status(url_prefix + "/status"), up(up) {}
};

CURRENT_STRUCT(RESTStatus) {
  CURRENT_FIELD(up, bool);

  CURRENT_CONSTRUCTOR(RESTStatus)(bool up = false) : up(up) {}
};

CURRENT_STRUCT(RESTError) {
  using details_t = std::map<std::string, std::string>;
  CURRENT_FIELD(name, std::string, "");
  CURRENT_FIELD(message, std::string, "");
  CURRENT_FIELD(details, Optional<details_t>);

  CURRENT_DEFAULT_CONSTRUCTOR(RESTError) {}
  CURRENT_CONSTRUCTOR(RESTError)(const std::string& name, const std::string& message) : name(name), message(message) {}
  CURRENT_CONSTRUCTOR(RESTError)
  (const std::string& name, const std::string& message, const details_t& details)
      : name(name), message(message), details(details) {}
};

CURRENT_STRUCT_T(RESTSubCollection) {
  CURRENT_FIELD(total, int64_t, 0);
  CURRENT_FIELD(preview, std::vector<T>);
};

CURRENT_STRUCT(RESTGenericResponse) {
  CURRENT_FIELD(success, bool, true);
  CURRENT_FIELD(message, Optional<std::string>);
  CURRENT_FIELD(error, Optional<RESTError>);

  CURRENT_DEFAULT_CONSTRUCTOR(RESTGenericResponse) {}
  CURRENT_CONSTRUCTOR(RESTGenericResponse)(bool success) : success(success) {}
  CURRENT_CONSTRUCTOR(RESTGenericResponse)
  (bool success, const std::string& message) : success(success), message(message) {}
  CURRENT_CONSTRUCTOR(RESTGenericResponse)
  (bool success, const std::string& message, const RESTError& error)
      : success(success), message(message), error(error) {}
  CURRENT_CONSTRUCTOR(RESTGenericResponse)(bool success, const RESTError& error) : success(success), error(error) {}
};

CURRENT_STRUCT(RESTResourceUpdateResponse, RESTGenericResponse) {
  CURRENT_FIELD(resource_url, Optional<std::string>);

  CURRENT_CONSTRUCTOR(RESTResourceUpdateResponse)(bool success = true) : SUPER(success) {}
  CURRENT_CONSTRUCTOR(RESTResourceUpdateResponse)(bool success, const std::string& message) : SUPER(success, message) {}
  CURRENT_CONSTRUCTOR(RESTResourceUpdateResponse)
  (bool success, const std::string& message, const std::string& resource_url)
      : SUPER(success, message), resource_url(resource_url) {}
  CURRENT_CONSTRUCTOR(RESTResourceUpdateResponse)
  (bool success, const std::string& message, const RESTError& error) : SUPER(success, message, error) {}
};

}  // namespace generic

namespace helpers {

inline generic::RESTGenericResponse ErrorResponseObject(const std::string& message, const generic::RESTError& error) {
  return generic::RESTGenericResponse(false, message, error);
}

inline generic::RESTGenericResponse ErrorResponseObject(const generic::RESTError& error) {
  return generic::RESTGenericResponse(false, error);
}

inline current::http::Response ErrorResponse(const generic::RESTError& error_object, net::HTTPResponseCodeValue code) {
  return current::http::Response(ErrorResponseObject(error_object), code);
}

inline generic::RESTError MethodNotAllowedError(const std::string& message, const std::string& requested_method) {
  return generic::RESTError("MethodNotAllowed", message, {{"requested_method", requested_method}});
}

inline generic::RESTError InvalidHeaderError(const std::string& message,
                                             const std::string& header,
                                             const std::string& value) {
  return generic::RESTError("InvalidHeader", message, {{"header", header}, {"header_value", value}});
}

inline generic::RESTError ParseJSONError(const std::string& message, const std::string& error_details) {
  return generic::RESTError("ParseJSONError", message, {{"error_details", error_details}});
};

inline generic::RESTError RequiredKeyIsMissingError(const std::string& message) {
  return generic::RESTError("RequiredKeyIsMissing", message);
}

inline generic::RESTError InvalidKeyError(const std::string& message) {
  return generic::RESTError("InvalidKey", message);
}

inline generic::RESTError InvalidKeyError(const std::string& message,
                                          const std::map<std::string, std::string>& details) {
  return generic::RESTError("InvalidKey", message, details);
}

inline generic::RESTError ResourceNotFoundError(const std::string& message,
                                                const std::map<std::string, std::string>& details) {
  return generic::RESTError("ResourceNotFound", message, details);
}

inline generic::RESTError ResourceAlreadyExistsError(const std::string& message,
                                                     const std::map<std::string, std::string>& details) {
  return generic::RESTError("ResourceAlreadyExists", message, details);
}

inline generic::RESTError ResourceWasModifiedError(const std::string& message,
                                                   std::chrono::microseconds requested,
                                                   std::chrono::microseconds last_modified) {
  return generic::RESTError("ResourceWasModifiedError",
                            message,
                            {{"requested_date", FormatDateTimeAsIMFFix(requested)},
                             {"requested_us", ToString(requested)},
                             {"resource_last_modified_date", FormatDateTimeAsIMFFix(last_modified)},
                             {"resource_last_modified_us", ToString(last_modified)}});
}

}  // namespace helpers

namespace simple {

CURRENT_STRUCT_T(SimpleRESTRecordResponse) {
  CURRENT_FIELD(success, bool, true);
  CURRENT_FIELD(url, std::string, "");
  CURRENT_FIELD(data, T);

  CURRENT_DEFAULT_CONSTRUCTOR_T(SimpleRESTRecordResponse) {}
  CURRENT_CONSTRUCTOR_T(SimpleRESTRecordResponse)(const std::string& url, const T& data) : url(url), data(data) {}
  CURRENT_CONSTRUCTOR_T(SimpleRESTRecordResponse)(const std::string& url, T&& data) : url(url), data(std::move(data)) {}
};

CURRENT_STRUCT(SimpleRESTContainerResponse, generic::RESTGenericResponse) {
  CURRENT_FIELD(url, std::string, "");
  CURRENT_FIELD(data, std::vector<std::string>);

  CURRENT_DEFAULT_CONSTRUCTOR(SimpleRESTContainerResponse) : SUPER(true) {}
  CURRENT_CONSTRUCTOR(SimpleRESTContainerResponse)
  (const std::string& url, const std::vector<std::string>& data) : SUPER(true), url(url), data(data) {}
  CURRENT_CONSTRUCTOR(SimpleRESTContainerResponse)
  (const std::string& url, std::vector<std::string>&& data) : SUPER(true), url(url), data(std::move(data)) {}
};

}  // namespace simple

namespace hypermedia {

using HypermediaRESTError = generic::RESTError;
using HypermediaRESTStatus = generic::RESTStatus;
using HypermediaRESTTopLevel = generic::RESTTopLevel;

template <typename T>
using HypermediaRESTSubCollection = generic::RESTSubCollection<T>;
using HypermediaRESTResourceUpdateResponse = generic::RESTResourceUpdateResponse;

CURRENT_STRUCT_T(HypermediaRESTSingleRecordResponse) {
  CURRENT_FIELD(success, bool, true);
  CURRENT_FIELD(url, std::string);
  CURRENT_FIELD(url_full, std::string);
  CURRENT_FIELD(url_brief, std::string);
  CURRENT_FIELD(url_directory, std::string);
  CURRENT_FIELD(data, T);

  CURRENT_DEFAULT_CONSTRUCTOR_T(HypermediaRESTSingleRecordResponse) {}
  CURRENT_CONSTRUCTOR_T(HypermediaRESTSingleRecordResponse)
  (const std::string& url, const std::string& url_directory, const T& data)
      : url(url), url_full(url + "?full"), url_brief(url + "?brief"), url_directory(url_directory), data(data) {}
};

CURRENT_STRUCT_T(HypermediaRESTFullCollectionRecord) {
  CURRENT_FIELD(url, std::string);
  CURRENT_FIELD(data, T);
  T& DataOrBriefByRef() { return data; }
};

CURRENT_STRUCT_T(HypermediaRESTBriefCollectionRecord) {
  CURRENT_FIELD(url, std::string);
  CURRENT_FIELD(brief, T);
  T& DataOrBriefByRef() { return brief; }
};

CURRENT_STRUCT_T(HypermediaRESTCollectionResponse) {
  CURRENT_FIELD(success, bool, true);
  CURRENT_FIELD(url, std::string);
  CURRENT_FIELD(url_directory, std::string);
  // TODO(dkorolev): `url_full_directory` for half-matrices? Tagging with #DIMA_FIXME.
  CURRENT_FIELD(i, uint64_t);
  CURRENT_FIELD(n, uint64_t);
  CURRENT_FIELD(total, uint64_t);
  CURRENT_FIELD(url_next_page, Optional<std::string>);
  CURRENT_FIELD(url_previous_page, Optional<std::string>);
  CURRENT_FIELD(data, std::vector<T>);
};

// The simplest way to pass universal dictionary/matrix key and entry to the templated Current struct.
template <typename KEY, typename ENTRY>
struct DetailedExportEntryHelper {
  using key_t = KEY;
  using entry_cref_t = const ENTRY&;
  key_t key;
  entry_cref_t entry_cref;
  DetailedExportEntryHelper(key_t key, entry_cref_t entry_cref) : key(key), entry_cref(entry_cref) {}
};

// Required for templated Current structs using `T::something` :(
template <typename T>
struct DetailedExportEntryHelperWrapper {
  using key_t = typename T::key_t;
  using entry_cref_t = typename T::entry_cref_t;
};

template <>
struct DetailedExportEntryHelperWrapper<current::reflection::DummyTemplateType> {
  using key_t = int;
  using entry_cref_t = int;
};

// `T` = `DetailedExportEntryHelper<KEY, ENTRY>`.
// NOTE: This structure uses const reference to avoid copying entries, thus serving as a "view" for
// serialization purposes only.
CURRENT_STRUCT_T(HypermediaRESTDetailedExportEntry) {
  CURRENT_FIELD(key, typename DetailedExportEntryHelperWrapper<T>::key_t);
  CURRENT_FIELD(timestamp_us, std::chrono::microseconds, std::chrono::microseconds(0));
  CURRENT_FIELD(timestamp_http, std::string);
  CURRENT_FIELD(data, typename DetailedExportEntryHelperWrapper<T>::entry_cref_t);
  CURRENT_CONSTRUCTOR_T(HypermediaRESTDetailedExportEntry)
  (std::chrono::microseconds timestamp, const T& helper)
      : key(helper.key),
        timestamp_us(timestamp),
        timestamp_http(FormatDateTimeAsIMFFix(timestamp)),
        data(helper.entry_cref) {}
};

using HypermediaRESTGenericResponse = generic::RESTGenericResponse;

}  // namespace hypermedia

using HypermediaRESTError = hypermedia::HypermediaRESTError;
using HypermediaRESTStatus = hypermedia::HypermediaRESTStatus;
using HypermediaRESTTopLevel = hypermedia::HypermediaRESTTopLevel;
using HypermediaRESTGenericResponse = hypermedia::HypermediaRESTGenericResponse;

// Expose helper functions into `current::storage::rest` as well for now. #DIMA_FIXME
using namespace helpers;

namespace cqs {

// clang-format off
CURRENT_STRUCT(CQSHandlerNotSpecified, generic::RESTGenericResponse) {
  CURRENT_DEFAULT_CONSTRUCTOR(CQSHandlerNotSpecified) : SUPER(false, "CQS handler not specified.") {}
};

CURRENT_STRUCT(CQSHandlerNotFound, generic::RESTGenericResponse) {
  CURRENT_DEFAULT_CONSTRUCTOR(CQSHandlerNotFound) : SUPER(false, "CQS handler not found.") {}
};

CURRENT_STRUCT(CQSCommandNeedsMasterStorage, generic::RESTGenericResponse) {
  CURRENT_DEFAULT_CONSTRUCTOR(CQSCommandNeedsMasterStorage) :
      SUPER(false, "CQS commands must be run on the master storage.") {}
};

CURRENT_STRUCT(CQSParseJSONException, generic::RESTGenericResponse) {
  CURRENT_CONSTRUCTOR(CQSParseJSONException)(const std::string& what) :
      SUPER(false, "CQS command or query HTTP body JSON parse error.", generic::RESTError("cqs_json_error", what)) {}
};

CURRENT_STRUCT(CQSParseURLException, generic::RESTGenericResponse) {
  CURRENT_CONSTRUCTOR(CQSParseURLException)(const std::string& what) :
      SUPER(false, "CQS command or query URL parameters parse error.", generic::RESTError("cqs_querystring_error", what)) {}
};

CURRENT_STRUCT(CQSBadRequest, generic::RESTGenericResponse) {
  CURRENT_DEFAULT_CONSTRUCTOR(CQSBadRequest) : SUPER(false, "Bad CQS request.") {}
};
// clang-format on

}  // namespace cqs

}  // namespace rest
}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_REST_TYPES_H
