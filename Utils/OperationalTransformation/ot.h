/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef UTILS_OPERATIONAL_TRANSFORMATION_H
#define UTILS_OPERATIONAL_TRANSFORMATION_H

#include "../../port.h"

#include "../../TypeSystem/Serialization/json.h"

#include <deque>
#include <codecvt>

namespace current {
namespace utils {
namespace ot {

struct OTParseException : Exception {
  using Exception::Exception;
};

static std::string AsUTF8String(const std::deque<wchar_t>& rope) {
  return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(std::wstring(rope.begin(), rope.end()));
}

template <typename PROCESSOR>
typename std::result_of<decltype(&PROCESSOR::Final)(PROCESSOR*, const std::deque<wchar_t>&)>::type OT(
    const std::string& json, PROCESSOR&& processor) {
  rapidjson::Document document;
  if (document.Parse<0>(&json[0]).HasParseError()) {
    CURRENT_THROW(OTParseException("Not a valid JSON."));
  }
  if (!document.IsObject()) {
    CURRENT_THROW(OTParseException("Not a JSON object."));
  }

  std::deque<wchar_t> rope;

  for (auto cit = document.MemberBegin(); cit != document.MemberEnd(); ++cit) {
    const auto& inner = cit->value;
    if (!inner.IsObject()) {
      CURRENT_THROW(
          OTParseException(std::string("The value for key `") + cit->name.GetString() + "` is not an object."));
    }
    if (!inner.HasMember("t")) {
      CURRENT_THROW(OTParseException(std::string("The object for key `") + cit->name.GetString() +
                                     "` does not contain the `t` field."));
    }
    if (!inner.HasMember("o")) {
      CURRENT_THROW(OTParseException(std::string("The object for key `") + cit->name.GetString() +
                                     "` does not contain the `o` field."));
    }
    const auto& timestamp_value = inner["t"];
    const auto& array_value = inner["o"];
    if (!timestamp_value.IsUint64()) {
      CURRENT_THROW(OTParseException(std::string("The `") + cit->name.GetString() + ".t` field is not an integer."));
    }
    if (!array_value.IsArray()) {
      CURRENT_THROW(OTParseException(std::string("The `") + cit->name.GetString() + ".o` field is not an array."));
    }

    const uint64_t timestamp = timestamp_value.GetUint64();
    int caret = 0;
    for (rapidjson::SizeType i = 0; i < static_cast<rapidjson::SizeType>(array_value.Size()); ++i) {
      const auto& element = array_value[i];
      if (element.IsString()) {
        const auto s = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(
            element.GetString(), element.GetString() + element.GetStringLength());
        rope.insert(rope.begin() + caret, s.begin(), s.end());
        caret += s.length();
      } else {
        if (!element.IsNumber()) {
          CURRENT_THROW(OTParseException("OT element is not a string or number."));
        }
        const int v = element.GetInt();
        if (v > 0) {
          caret += v;
        } else {
          rope.erase(rope.begin() + caret, rope.begin() + caret + (-v));
        }
      }
    }

    if (!processor.Intermediate(std::chrono::microseconds(timestamp * 1000), rope)) {
      // Early return if requested by the user.
      return processor.EmptyOutput();
    }
  }

  return processor.Final(rope);
}

inline std::string OT(const std::string& json) {
  struct PassthroughProcessor {
    bool Intermediate(std::chrono::microseconds, const std::deque<wchar_t>&) const { return true; }
    std::string Final(const std::deque<wchar_t>& rope) const { return AsUTF8String(rope); }
    std::string EmptyOutput() const { return ""; }
  };
  return OT(json, PassthroughProcessor());
}

}  // namespace current::utils::ot
}  // namespace current::utils
}  // namespace current

#endif  // UTILS_OPERATIONAL_TRANSFORMATION_H
