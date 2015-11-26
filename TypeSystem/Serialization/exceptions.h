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
#include "../../Bricks/exception.h"
#include "../../Bricks/strings/strings.h"

#define RAPIDJSON_HAS_STDSTRING 1
#include "../../3rdparty/rapidjson/document.h"
#include "../../3rdparty/rapidjson/prettywriter.h"
#include "../../3rdparty/rapidjson/streamwrapper.h"

namespace current {
namespace serialization {

struct TypeSystemParseJSONException : Exception {
  using Exception::Exception;
};

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
      } catch (const std::exception& e) {
        return "field can not be parsed.";
      }
    } else {
      return "missing field.";
    }
  }
};

struct InvalidJSONException : TypeSystemParseJSONException {
  explicit InvalidJSONException(const std::string& json) : TypeSystemParseJSONException(json) {}
};

struct BinarySerializationException : Exception {
  using Exception::Exception;
};

struct BinarySaveToStreamException : BinarySerializationException {
  using BinarySerializationException::BinarySerializationException;
  BinarySaveToStreamException(const size_t bytes_to_write, const size_t actually_wrote)
      : BinarySerializationException("Failed to write " + bricks::strings::ToString(bytes_to_write) +
                                     " bytes, wrote only " + bricks::strings::ToString(actually_wrote) + '.') {}
};

struct BinaryLoadFromStreamException : BinarySerializationException {
  using BinarySerializationException::BinarySerializationException;
  BinaryLoadFromStreamException(const size_t bytes_to_read, const size_t actually_read)
      : BinarySerializationException("Failed to read " + bricks::strings::ToString(bytes_to_read) +
                                     " bytes, read only " + bricks::strings::ToString(actually_read) + '.') {}
};

}  // namespace serialization
}  // namespace current

using current::serialization::InvalidJSONException;
using current::serialization::TypeSystemParseJSONException;

#endif  // TYPE_SYSTEM_SERIALIZATION_EXCEPTIONS_H
