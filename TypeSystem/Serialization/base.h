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
#include "rapidjson.h"

#include "../../Bricks/template/pod.h"  // `current::copy_free`.

namespace current {
namespace serialization {

template <class SERIALIZER, typename T, typename ENABLE = void>
struct SerializeImpl;

template <class SERIALIZER, typename T>
inline void Serialize(SERIALIZER&& serializer, T&& x) {
  SerializeImpl<current::decay<SERIALIZER>, current::decay<T>>::DoSerialize(
      std::forward<SERIALIZER>(serializer), std::forward<T>(x));
}

namespace json {

template <typename T>
struct JSONValueAssignerImpl {
  static void AssignValue(rapidjson::Value& destination, current::copy_free<T> value) { destination = value; }
};

template <class JSON_FORMAT>
class JSONStringifier final {
 public:
  JSONStringifier() {
    current_ = &document_;  // Can't assign in the initializer list, `current_` is the field before `document_`.
  }

  rapidjson::Value& Current() { return *current_; }
  rapidjson::Document::AllocatorType& Allocator() { return document_.GetAllocator(); }

  template <typename T>
  void operator=(T&& x) {
    JSONValueAssignerImpl<current::decay<T>>::AssignValue(*current_, std::forward<T>(x));
  }

  // Serialize another object, in an inner scope. The object is guaranteed to result in a valid value.
  template <typename T>
  void Inner(rapidjson::Value* inner_value, T&& x) {
    std::swap(current_, inner_value);
    Serialize(*this, std::forward<T>(x));
    std::swap(current_, inner_value);
  }

  // Serialize another object, in an inner scope. The object may end up a no-op, which should be ignored.
  // Example: A `Variant` or `Optional` in the `Minimalistic` format.
  void MarkAsAbsentValue() { current_ = nullptr; }
  template <typename T>
  bool MaybeInner(rapidjson::Value* inner_value, T&& x) {
    rapidjson::Value* previous = current_;
    current_ = inner_value;
    Serialize(*this, std::forward<T>(x));
    if (current_) {
      current_ = previous;
      return true;
    } else {
      current_ = previous;
      return false;
    }
  }

  std::string ResultingJSON() const {
    rapidjson::StringBuffer string_buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(string_buffer);
    document_.Accept(writer);
    return string_buffer.GetString();
  }

 private:
  rapidjson::Value* current_;
  rapidjson::Document document_;
};

enum class JSONVariantStyle : int { Current, Simple, NewtonsoftFSharp };

template <JSONVariantStyle>
struct JSONVariantStyleUseNulls {
  constexpr static bool value = true;
};
template <>
struct JSONVariantStyleUseNulls<JSONVariantStyle::Simple> {
  constexpr static bool value = false;
};

struct JSONFormat {
  struct Current {
    constexpr static JSONVariantStyle variant_style = JSONVariantStyle::Current;
  };
  struct Minimalistic {
    constexpr static JSONVariantStyle variant_style = JSONVariantStyle::Simple;
  };
  struct JavaScript {
    constexpr static JSONVariantStyle variant_style = JSONVariantStyle::Simple;
  };
  struct NewtonsoftFSharp {
    constexpr static JSONVariantStyle variant_style = JSONVariantStyle::NewtonsoftFSharp;
  };
};

template <class>
struct JSONVariantTypeIDInEmptyKey {
  constexpr static bool value = false;
};
template <>
struct JSONVariantTypeIDInEmptyKey<JSONFormat::Current> {
  constexpr static bool value = true;
};

template <class>
struct JSONVariantTypeNameInDollarKey {
  constexpr static bool value = false;
};
template <>
struct JSONVariantTypeNameInDollarKey<JSONFormat::JavaScript> {
  constexpr static bool value = true;
};

template <class J>
struct JSONPatcher {
  using J::variant_style;
};

template <class J>
struct JSONPatchMode {
  constexpr static bool value = false;
};

template <class J>
struct JSONPatchMode<JSONPatcher<J>> {
  constexpr static bool value = true;
};

namespace load {

template <typename, typename JSON_FORMAT, typename ENABLE = void>
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
