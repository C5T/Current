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

#ifndef CURRENT_SERIALIZATION_JSON_H
#define CURRENT_SERIALIZATION_JSON_H

#include "../Bricks/template/decay.h"
#include "../Reflection/reflection.h"

// TODO(dkorolev): Use RapidJSON from outside Cereal.
#include "../3rdparty/cereal/include/external/rapidjson/document.h"
#include "../3rdparty/cereal/include/external/rapidjson/prettywriter.h"
#include "../3rdparty/cereal/include/external/rapidjson/genericstream.h"

namespace current {
namespace serialization {

template <typename T>
struct SaveIntoJSONImpl;

template <typename T>
struct AssignToRapidJSONValueImpl {
  static void WithDedicatedStringTreatment(rapidjson::Value& destination, const T& value) {
    destination = value;
  }
};

template <>
struct AssignToRapidJSONValueImpl<std::string> {
  static void WithDedicatedStringTreatment(rapidjson::Value& destination, const std::string& value) {
    destination.SetString(value.c_str(), value.length());
  }
};

template <typename T>
void AssignToRapidJSONValue(rapidjson::Value& destination, const T& value) {
  AssignToRapidJSONValueImpl<T>::WithDedicatedStringTreatment(destination, value);
}

#define CURRENT_DECLARE_PRIMITIVE_TYPE(unused_typeid_index, cpp_type, unused_current_type) \
  template <>                                                                              \
  struct SaveIntoJSONImpl<cpp_type> {                                                      \
    static void Go(rapidjson::Value& destination,                                          \
                   rapidjson::Document::AllocatorType&,                                    \
                   const cpp_type& value) {                                                \
      AssignToRapidJSONValue(destination, value);                                          \
    }                                                                                      \
  };
#include "../Reflection/primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

template <typename T>
struct SaveIntoJSONImpl<std::vector<T>> {
  static void Go(rapidjson::Value& destination,
                 rapidjson::Document::AllocatorType& allocator,
                 const std::vector<T>& value) {
    destination.SetArray();
    rapidjson::Value element_to_push;
    for (const auto& element : value) {
      SaveIntoJSONImpl<T>::Go(element_to_push, allocator, element);
      destination.PushBack(element_to_push, allocator);
    }
  }
};

// TODO(dkorolev): A smart `enable_if` to not treat any non-primitive type as a `CURRENT_STRUCT`?
template <typename T>
struct SaveIntoJSONImpl {
  struct Visitor {
    rapidjson::Value& destination_;
    rapidjson::Document::AllocatorType& allocator_;

    Visitor(rapidjson::Value& destination, rapidjson::Document::AllocatorType& allocator)
        : destination_(destination), allocator_(allocator) {}

    // IMPORTANT: Pass in `const char* name`, as `const std::string& name`
    // would fail memory-allocation-wise due to over-smartness of RapidJSON.
    template <typename U>
    void operator()(const char* name, const U& value) const {
      rapidjson::Value placeholder;
      SaveIntoJSONImpl<U>::Go(placeholder, allocator_, value);
      destination_.AddMember(name, placeholder, allocator_);
    }
  };

  static void Go(rapidjson::Value& destination, rapidjson::Document::AllocatorType& allocator, const T& value) {
    destination.SetObject();

    Visitor serializer(destination, allocator);
    current::reflection::VisitAllFields<bricks::decay<T>, current::reflection::FieldNameAndImmutableValue>::
        WithObject(value, serializer);
  }
};

template <typename T>
std::string JSON(const T& value) {
  rapidjson::Document document;
  rapidjson::Value& destination = document;

  SaveIntoJSONImpl<T>::Go(destination, document.GetAllocator(), value);

  std::ostringstream os;
  auto stream = rapidjson::GenericWriteStream(os);
  auto writer = rapidjson::Writer<rapidjson::GenericWriteStream>(stream);
  document.Accept(writer);

  return os.str();
}

}  // namespace serialization
}  // namespace current

// Inject into global namespace.
using current::serialization::JSON;

#endif  // CURRENT_SERIALIZATION_JSON_H
