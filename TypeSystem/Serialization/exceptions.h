/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef TYPE_SYSTEM_SERIALIZATION_EXCEPTIONS_H
#define TYPE_SYSTEM_SERIALIZATION_EXCEPTIONS_H

#include "../../port.h"

#include "exceptions_base.h"

#include "rapidjson.h"

namespace current {
namespace serialization {
namespace json {

struct JSONSchemaException : TypeSystemParseJSONException {
  const std::string expected_;
  const std::string actual_;
  JSONSchemaException(const std::string& expected, rapidjson::Value* value, const std::string& path)
      : TypeSystemParseJSONException("Expected " +
                                     (expected + (path.empty() ? "" : " for `" + path.substr(1u) + "`")) +
                                     ", got: " + NonThrowingFormatRapidJSONValueAsString(value)) {}
  static std::string NonThrowingFormatRapidJSONValueAsString(
      rapidjson::Value* value) {  // Attempt to generate a human-readable description of the part of the JSON,
    // that has been parsed but is of wrong schema.
    if (value) {
      try {
        std::ostringstream os;
        rapidjson::OStreamWrapper stream(os);
        rapidjson::Writer<rapidjson::OStreamWrapper> writer(stream);
        rapidjson::Document document;
        if (value->IsObject() || value->IsArray()) {
          // Objects and arrays can be dumped directly.
          value->Accept(writer);
          return os.str();
        } else {
          // Every other type of value has to be wrapped into an object or an array.
          // Hack to extract the actual value: wrap into an array and peel off the '[' and ']'. -- D.K.
          document.SetArray();
          document.PushBack(*value, document.GetAllocator());
          document.Accept(writer);
          const std::string result = os.str();
          return result.substr(1u, result.length() - 2u);
        }
      } catch (const std::exception&) {     // LCOV_EXCL_LINE
        return "field can not be parsed.";  // LCOV_EXCL_LINE
      }
    } else {
      return "missing field.";
    }
  }
};

}  // namespace current::serialization::json
}  // namespace current::serialization
}  // namespace current

#endif  // TYPE_SYSTEM_SERIALIZATION_EXCEPTIONS_H
