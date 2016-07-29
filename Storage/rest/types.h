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

#include "../../TypeSystem/struct.h"
#include "../../TypeSystem/optional.h"

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

CURRENT_STRUCT_T(HypermediaRESTSubCollection) {
  CURRENT_FIELD(total, int64_t, 0);
  CURRENT_FIELD(preview, std::vector<T>);
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

}  // namespace rest
}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_REST_TYPES_H
