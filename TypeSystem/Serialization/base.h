/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_BASE_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_BASE_H

#include <iostream>

#include "exceptions.h"

#define RAPIDJSON_HAS_STDSTRING 1
#include "../../3rdparty/rapidjson/document.h"
#include "../../3rdparty/rapidjson/prettywriter.h"
#include "../../3rdparty/rapidjson/streamwrapper.h"

namespace current {
namespace serialization {
namespace json {

enum class JSONFormat : int { Current, Minimalistic, NewtonsoftFSharp };

namespace save {

template <typename T>
struct AssignToRapidJSONValueImpl {
  static void WithDedicatedTreatment(rapidjson::Value& destination, const T& value) { destination = value; }
};

template <typename T>
void AssignToRapidJSONValue(rapidjson::Value& destination, const T& value) {
  AssignToRapidJSONValueImpl<T>::WithDedicatedTreatment(destination, value);
}

template <typename, JSONFormat, typename Enable = void>
struct SaveIntoJSONImpl;

}  // namespace save

namespace load {

template <typename, JSONFormat, typename Enable = void>
struct LoadFromJSONImpl;

}  // namespace load
}  // namespace json

namespace binary {

// Using platform-independent size type for binary (de)serialization.
typedef uint64_t BINARY_FORMAT_SIZE_TYPE;
typedef uint8_t BINARY_FORMAT_BOOL_TYPE;

namespace save {

inline void SaveSizeIntoBinary(std::ostream& ostream, const size_t size) {
  const BINARY_FORMAT_SIZE_TYPE save_size = size;
  const size_t bytes_written =
      ostream.rdbuf()->sputn(reinterpret_cast<const char*>(&save_size), sizeof(BINARY_FORMAT_SIZE_TYPE));
  if (bytes_written != sizeof(BINARY_FORMAT_SIZE_TYPE)) {
    throw BinarySaveToStreamException(sizeof(BINARY_FORMAT_SIZE_TYPE), bytes_written);  // LCOV_EXCL_LINE
  }
};

template <typename, typename Enable = void>
struct SaveIntoBinaryImpl;

}  // namespace save

namespace load {

inline BINARY_FORMAT_SIZE_TYPE LoadSizeFromBinary(std::istream& istream) {
  BINARY_FORMAT_SIZE_TYPE result;
  const size_t bytes_read =
      istream.rdbuf()->sgetn(reinterpret_cast<char*>(&result), sizeof(BINARY_FORMAT_SIZE_TYPE));
  if (bytes_read != sizeof(BINARY_FORMAT_SIZE_TYPE)) {
    throw BinaryLoadFromStreamException(sizeof(BINARY_FORMAT_SIZE_TYPE), bytes_read);  // LCOV_EXCL_LINE
  }
  return result;
}

template <typename, typename Enable = void>
struct LoadFromBinaryImpl;

}  // namespace load
}  // namespace binary

}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_BASE_H
