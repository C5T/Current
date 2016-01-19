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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_H

#include "exceptions.h"

#define RAPIDJSON_HAS_STDSTRING 1
#include "../../3rdparty/rapidjson/document.h"
#include "../../3rdparty/rapidjson/prettywriter.h"
#include "../../3rdparty/rapidjson/streamwrapper.h"

namespace current {
namespace serialization {
namespace json {

// Current: as human- and JavaScript-readable as possible, with strong struct signature checking.
// Used internally, and is the default for exposed API endpoints.
// * `CURRENT_STRUCT`-s as objects with field names as keys.
// * `map<string, X>` as `{"k1":<v1>, "k2":<v2>, ...}`, `map<K, V>` as `[[<k1>, <v1>], [<k2>, <v2>], ...]`.
// * `Variant<A, B, C> x = A(...)` as `{"A":<body>, "":<type ID of A>}`.
//
// Minimalistic: same as `Current` above except for treating `Variant`s.
// Less strict. Used primarily to import data into Current from a trusted source, ex. a local Python script.
// * Does not serialize Type ID into the empty key.
// * Relies on the name of the top-level object only when doing `Variant` deserialization.
//
// NewtonsoftFSharp: the format friendly with `import Newtonsoft.JSON` from F#.
// Used to communicate with F# and/or Visual Studio.
// Difference from `Current`:
// * Variant types are stored as `{"Case":"A", "Fields":[ { <fields of A> } ]}`.
// * TODO(dkorolev): Confirm `map<K, V>` does what it should. Untested as of now; `vector<T>` works fine.

enum class JSONFormat : int { Current, Minimalistic, NewtonsoftFSharp };

template <typename T>
struct AssignToRapidJSONValueImpl {
  static void WithDedicatedTreatment(rapidjson::Value& destination, const T& value) { destination = value; }
};

template <typename T>
void AssignToRapidJSONValue(rapidjson::Value& destination, const T& value) {
  AssignToRapidJSONValueImpl<T>::WithDedicatedTreatment(destination, value);
}

namespace save {

template <typename, JSONFormat, typename Enable = void>
struct SaveIntoJSONImpl;

}  // namespace save

namespace load {

template <typename T, JSONFormat J, class Enable = void>
struct LoadFromJSONImpl;

}  // namespace load

}  // namespace json
}  // namespace serialization
}  // namespace current

// Basic types.
#include "types/primitive.h"
#include "types/chrono.h"
#include "types/enum.h"

//STL containers.
#include "types/vector.h"
#include "types/pair.h"
#include "types/map.h"

// Current types.
#include "types/current_typeid.h"
#include "types/current_struct.h"
#include "types/optional.h"
#include "types/variant.h"

namespace current {
namespace serialization {
namespace json {

template <JSONFormat J, typename T>
std::string CreateJSONViaRapidJSON(const T& value) {
  rapidjson::Document document;
  rapidjson::Value& destination = document;

  save::SaveIntoJSONImpl<T, J>::Save(destination, document.GetAllocator(), value);

  std::ostringstream os;
  rapidjson::OStreamWrapper stream(os);
  rapidjson::Writer<rapidjson::OStreamWrapper> writer(stream);
  document.Accept(writer);

  return os.str();
}

template <JSONFormat J, typename T>
void ParseJSONViaRapidJSON(const std::string& json, T& destination) {
  rapidjson::Document document;

  if (document.Parse(json.c_str()).HasParseError()) {
    throw InvalidJSONException(json);
  }

  load::LoadFromJSONImpl<T, J>::Load(&document, destination, "");
}

template <JSONFormat J = JSONFormat::Current, typename T>
inline std::string JSON(const T& source) {
  return CreateJSONViaRapidJSON<J>(source);
}

template <JSONFormat J = JSONFormat::Current>
inline std::string JSON(const char* special_case_bare_c_string) {
  return JSON<J>(std::string(special_case_bare_c_string));
}

template <typename T, JSONFormat J = JSONFormat::Current>
inline void ParseJSON(const std::string& source, T& destination) {
  try {
    ParseJSONViaRapidJSON<J>(source, destination);
    CheckIntegrity(destination);
  } catch (UninitializedVariant) {
    throw JSONUninitializedVariantObjectException();
  }
}

template <typename T, JSONFormat J = JSONFormat::Current>
inline T ParseJSON(const std::string& source) {
  try {
    T result;
    ParseJSONViaRapidJSON<J>(source, result);
    CheckIntegrity(result);
    return result;
  } catch (UninitializedVariant) {
    throw JSONUninitializedVariantObjectException();
  }
}

}  // namespace json
}  // namespace serialization
}  // namespace current


// Inject into global namespace.
using current::serialization::json::JSON;
using current::serialization::json::ParseJSON;

using current::serialization::json::JSONFormat;

using current::serialization::json::TypeSystemParseJSONException;

using current::serialization::json::JSONSchemaException;
using current::serialization::json::InvalidJSONException;
using current::serialization::json::JSONUninitializedVariantObjectException;

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_H
