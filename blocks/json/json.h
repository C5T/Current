/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2020 Dmitry "Dima" Korolev, <dmitry.korolev@gmail.com>.

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

// A slow yet clean `CURRENT_STRUCT` / `CURRET_VARIANT` wrapper over `rapidjson`.

#ifndef BLOCKS_JSON_H
#define BLOCKS_JSON_H

#include <ostream>

#include "../../typesystem/serialization/json.h"
#include "../../typesystem/struct.h"
#include "../../typesystem/variant.h"

namespace current {
namespace json {

CURRENT_FORWARD_DECLARE_STRUCT(JSONString);
CURRENT_FORWARD_DECLARE_STRUCT(JSONNumber);
CURRENT_FORWARD_DECLARE_STRUCT(JSONBoolean);
CURRENT_FORWARD_DECLARE_STRUCT(JSONNull);
CURRENT_FORWARD_DECLARE_STRUCT(JSONObject);
CURRENT_FORWARD_DECLARE_STRUCT(JSONArray);

CURRENT_VARIANT(JSONValue, JSONString, JSONNumber, JSONBoolean, JSONNull, JSONObject, JSONArray);

template <class T>
inline void AppendToJSON(std::ostream& os, const T& json_value) {
  json_value.DoAppendToJSON(os);
}

struct AppendToJSONImpl final {
  std::ostream& os;
  AppendToJSONImpl(std::ostream& os) : os(os) {}
  template <class T>
  void operator()(const T& json_value) {
    json_value.DoAppendToJSON(os);
  }
};

template <>
inline void AppendToJSON(std::ostream& os, const JSONValue& json_value) {
  json_value.Call(AppendToJSONImpl(os));
}

template <class T>
inline std::string AsJSON(const T& json_value) {
  std::ostringstream oss;
  AppendToJSON(oss, json_value);
  return oss.str();
}

CURRENT_STRUCT(JSONString) {
  CURRENT_FIELD(string, std::string);
  CURRENT_CONSTRUCTOR(JSONString)(std::string string = "") : string(std::move(string)) {}
  operator std::string() const { return string; }
  void DoAppendToJSON(std::ostream & os) const { os << JSON(string); }
};

CURRENT_STRUCT(JSONNumber) {
  CURRENT_FIELD(number, double);
  CURRENT_CONSTRUCTOR(JSONNumber)(double number = 0) : number(number) {}
  operator double() const { return number; }
  void DoAppendToJSON(std::ostream & os) const {
    if (number == std::floor(number)) {
      if (number < 0) {
        os << JSON(static_cast<int64_t>(number));
      } else {
        os << JSON(static_cast<uint64_t>(number));
      }
    } else {
      os << JSON(number);
    }
  }
};

CURRENT_STRUCT(JSONBoolean) {
  CURRENT_FIELD(boolean, bool);
  CURRENT_CONSTRUCTOR(JSONBoolean)(bool boolean = false) : boolean(boolean) {}
  operator bool() const { return boolean; }
  void DoAppendToJSON(std::ostream & os) const { os << JSON(boolean); }
};

// clang-format off
CURRENT_STRUCT(JSONNull) {
  void DoAppendToJSON(std::ostream& os) const {
    os << "null";
  }
};
// clang-format on

CURRENT_STRUCT(JSONArray) {
  CURRENT_FIELD(elements, std::vector<JSONValue>);
  size_t size() const { return elements.size(); }
  bool empty() const { return elements.empty(); }
  void push_back(JSONValue value) { elements.push_back(std::move(value)); }
  const JSONValue& operator[](size_t i) const {
    static JSONValue null((JSONNull()));
    return i < elements.size() ? elements[i] : null;
  }
  void DoAppendToJSON(std::ostream & os) const {
    os << '[';
    bool first = true;
    for (const JSONValue& element : elements) {
      if (first) {
        first = false;
      } else {
        os << ',';
      }
      AppendToJSON(os, element);
    }
    os << ']';
  }
  using iterator = typename std::vector<JSONValue>::iterator;
  iterator begin() { return elements.begin(); }
  iterator end() { return elements.end(); }
  using const_iterator = typename std::vector<JSONValue>::const_iterator;
  const_iterator begin() const { return elements.begin(); }
  const_iterator end() const { return elements.end(); }
};

CURRENT_STRUCT(JSONObject) {
  CURRENT_FIELD(fields, (std::unordered_map<std::string, JSONValue>));
  CURRENT_FIELD(keys, std::vector<std::string>);
  size_t size() const { return keys.size(); }
  bool empty() const { return keys.empty(); }
  bool HasKey(const std::string& key) const { return fields.count(key); }
  const JSONValue& operator[](const std::string& key) const {
    static JSONValue null((JSONNull()));
    const auto cit = fields.find(key);
    return cit != fields.end() ? cit->second : null;
  }
  JSONObject& push_back(const std::string& name, const JSONValue& value) {
    auto it_field = fields.find(name);
    if (it_field == std::end(fields)) {
      fields.emplace(name, value);
      keys.push_back(name);
    } else {
      it_field->second = value;
      auto it_list = std::find(std::begin(keys), std::end(keys), name);
      std::rotate(it_list, it_list + 1u, std::end(keys));
    }
    return *this;
  }
  JSONObject& erase(const std::string& name) {
    keys.erase(std::remove(keys.begin(), keys.end(), name), keys.end());
    fields.erase(name);
    return *this;
  }
  void DoAppendToJSON(std::ostream & os) const {
    os << '{';
    bool first = true;
    for (const std::string& key : keys) {
      if (first) {
        first = false;
      } else {
        os << ',';
      }
      os << JSON(key) << ':';
      AppendToJSON(os, fields.at(key));
    }
    os << '}';
  }
  struct const_element final {
    const std::string& key;
    const JSONValue& value;
    const_element(const std::string& key, const JSONValue& value) : key(key), value(value) {}
  };
  struct const_iterator final {
    const JSONObject& self;
    std::vector<std::string>::const_iterator cit;
    const_iterator(const JSONObject& self, std::vector<std::string>::const_iterator cit) : self(self), cit(cit) {}
    bool operator==(const const_iterator& rhs) const { return cit == rhs.cit; }
    bool operator!=(const const_iterator& rhs) const { return cit != rhs.cit; }
    const_element operator*() const { return const_element(*cit, self.fields.at(*cit)); }
    void operator++() { ++cit; }
  };
  const_iterator begin() const { return const_iterator(*this, keys.begin()); }
  const_iterator end() const { return const_iterator(*this, keys.end()); }
};

inline JSONValue ParseJSONUniversally(rapidjson::Value& value) {
  if (value.IsString()) {
    JSONString retval;
    retval.string.assign(value.GetString(), value.GetStringLength());
    return retval;
  } else if (value.IsNumber()) {
    JSONNumber retval;
    retval.number = value.GetDouble();
    return retval;
  } else if (value.IsBool()) {
    JSONBoolean retval;
    retval.boolean = value.GetBool();
    return retval;
  } else if (value.IsNull()) {
    return JSONNull();
  } else if (value.IsObject()) {
    JSONObject retval;
    for (rapidjson::Value::MemberIterator cit = value.MemberBegin(); cit != value.MemberEnd(); ++cit) {
      std::string key;
      key.assign(cit->name.GetString(), cit->name.GetStringLength());
      retval.keys.push_back(key);
      retval.fields.emplace(std::move(key), ParseJSONUniversally(cit->value));
    }
    return retval;
  } else if (value.IsArray()) {
    JSONArray retval;
    retval.elements.reserve(value.Size());
    for (rapidjson::SizeType i = 0; i < static_cast<rapidjson::SizeType>(value.Size()); ++i) {
      retval.elements.push_back(ParseJSONUniversally(value[i]));
    }
    return retval;
  } else {
    CURRENT_THROW(TypeSystemParseJSONException());
  }
}

inline JSONValue ParseJSONUniversally(rapidjson::Value* value) { return ParseJSONUniversally(*value); }

inline JSONValue ParseJSONUniversally(const std::string& json) {
  rapidjson::Document document;
  if (document.Parse(json).HasParseError()) {
    CURRENT_THROW(TypeSystemParseJSONException());
  }
  return ParseJSONUniversally(document);
}

}  // namespace current::json
}  // namespace current

#endif  // BLOCKS_JSON_H
