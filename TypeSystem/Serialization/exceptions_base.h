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

#ifndef TYPE_SYSTEM_SERIALIZATION_EXCEPTIONS_BASE_H
#define TYPE_SYSTEM_SERIALIZATION_EXCEPTIONS_BASE_H

#include "../exceptions.h"
#include "../../Bricks/strings/strings.h"

namespace current {
namespace serialization {

namespace json {

struct TypeSystemParseJSONException : Exception {
  using Exception::Exception;
};

struct RapidJSONAssertionFailedException : TypeSystemParseJSONException {
  using TypeSystemParseJSONException::TypeSystemParseJSONException;
};

struct InvalidJSONException : TypeSystemParseJSONException {
  explicit InvalidJSONException(const std::string& json) : TypeSystemParseJSONException(json) {}
};

struct JSONUninitializedVariantObjectException : TypeSystemParseJSONException {};

}  // namepsace current::serialization::json

namespace binary {

struct BinarySerializationException : Exception {
  using Exception::Exception;
};

// LCOV_EXCL_START
struct BinarySaveToStreamException : BinarySerializationException {
  using BinarySerializationException::BinarySerializationException;
  BinarySaveToStreamException(const size_t bytes_to_write, const size_t actually_wrote)
      : BinarySerializationException("Failed to write " + current::ToString(bytes_to_write) +
                                     " bytes, wrote only " + current::ToString(actually_wrote) + '.') {}
};
// LCOV_EXCL_STOP

struct BinaryLoadFromStreamException : BinarySerializationException {
  using BinarySerializationException::BinarySerializationException;
  BinaryLoadFromStreamException(const size_t bytes_to_read, const size_t actually_read)
      : BinarySerializationException("Failed to read " + current::ToString(bytes_to_read) +
                                     " bytes, read only " + current::ToString(actually_read) + '.') {}
};

}  // namespace current::serialization::binary

}  // namespace current::serialization
}  // namespace current

using current::serialization::json::InvalidJSONException;
using current::serialization::json::TypeSystemParseJSONException;
using current::serialization::json::RapidJSONAssertionFailedException;
using current::serialization::json::JSONUninitializedVariantObjectException;

using current::serialization::binary::BinarySaveToStreamException;
using current::serialization::binary::BinaryLoadFromStreamException;

#endif  // TYPE_SYSTEM_SERIALIZATION_EXCEPTIONS_BASE_H
