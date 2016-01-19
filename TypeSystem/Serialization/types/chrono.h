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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_VECTOR_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_VECTOR_H

#include <chrono>

#include "../json.h"

namespace current {
namespace serialization {
namespace json {

template <>
struct AssignToRapidJSONValueImpl<std::chrono::microseconds> {
  static void WithDedicatedTreatment(rapidjson::Value& destination, const std::chrono::microseconds& value) {
    destination.SetInt64(value.count());
  }
};

template <>
struct AssignToRapidJSONValueImpl<std::chrono::milliseconds> {
  static void WithDedicatedTreatment(rapidjson::Value& destination, const std::chrono::milliseconds& value) {
    destination.SetInt64(value.count());
  }
};

namespace save {

}  // namespace save

namespace load {

template <JSONFormat J>
struct LoadFromJSONImpl<std::chrono::milliseconds, J> {
  static void Load(rapidjson::Value* source, std::chrono::milliseconds& destination, const std::string& path) {
    uint64_t value_as_uint64;
    LoadFromJSONImpl<uint64_t, J>::Load(source, value_as_uint64, path);
    destination = std::chrono::milliseconds(value_as_uint64);
  }
};

template <JSONFormat J>
struct LoadFromJSONImpl<std::chrono::microseconds, J> {
  static void Load(rapidjson::Value* source, std::chrono::microseconds& destination, const std::string& path) {
    uint64_t value_as_uint64;
    LoadFromJSONImpl<uint64_t, J>::Load(source, value_as_uint64, path);
    destination = std::chrono::microseconds(value_as_uint64);
  }
};

}  // namespace load

}  // namespace json
}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_VECTOR_H


