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

#ifndef BRICKS_STRINGS_UTIL_H
#define BRICKS_STRINGS_UTIL_H

#include <algorithm>
#include <cstring>
#include <sstream>
#include <string>

#include "is_string_type.h"

#include "../template/enable_if.h"
#include "../template/decay.h"

namespace current {
namespace strings {

template <typename DECAYED_T, bool IS_ENUM>
struct ToStringImpl {
  static ENABLE_IF<std::is_pod<DECAYED_T>::value, std::string> DoIt(DECAYED_T value) {
    return std::to_string(value);
  }
};

template <typename DECAYED_T>
struct ToStringImpl<DECAYED_T, true> {
  static ENABLE_IF<std::is_pod<DECAYED_T>::value, std::string> DoIt(DECAYED_T value) {
    return std::to_string(static_cast<typename std::underlying_type<DECAYED_T>::type>(value));
  }
};

template <>
struct ToStringImpl<std::string, false> {
  template <typename T>
  static std::string DoIt(T&& string) {
    return string;
  }
};

template <>
struct ToStringImpl<char*, false> {  // Decayed type in template parameters list.
  static std::string DoIt(const char* string) { return string; }
};

template <int N>
struct ToStringImpl<char[N], false> {  // Decayed type in template parameters list.
  static std::string DoIt(const char string[N]) {
    return std::string(string, string + N - 1);  // Do not include the '\0' character.
  }
};

template <>
struct ToStringImpl<char, false> {
  static std::string DoIt(char c) { return std::string(1u, c); }
};

template <>
struct ToStringImpl<bool, false> {
  static std::string DoIt(bool b) { return b ? "true" : "false"; }
};

// Keep the lowercase name to possibly act as a replacement for `std::to_string`.
template <typename T>
inline std::string to_string(T&& something) {
  using DECAYED_T = current::decay<T>;
  return ToStringImpl<DECAYED_T, std::is_enum<DECAYED_T>::value>::DoIt(something);
}

template <typename T>
inline std::string to_string(std::reference_wrapper<T> something) {
  return ToStringImpl<T, std::is_enum<T>::value>::DoIt(something.get());
}

// Use camel-case `ToString()` within Bricks.
template <typename T>
inline std::string ToString(T&& something) {
  return current::strings::to_string(std::forward<T>(something));
}

// Special case for `std::pair<>`. For Storage REST only for now.
// TODO(dkorolev) + TODO(mzhurovich): Unify `ToString()` and `JSON()` under one `Serialize()` w/ policies?
template <typename FST, typename SND>
struct ToStringImpl<std::pair<FST, SND>, false> {
  static std::string DoIt(const std::pair<FST, SND>& pair) {
    return ToString(pair.first) + ':' + ToString(pair.second);
  }
};

template <typename T_INPUT, typename T_OUTPUT, bool IS_ENUM>
struct FromStringEnumImpl;

template <typename T_INPUT, typename T_OUTPUT>
struct FromStringEnumImpl<T_INPUT, T_OUTPUT, false> {
  template <typename T>
  static const T_OUTPUT& Go(T&& input, T_OUTPUT& output) {
    std::istringstream is(input);
    if (!(is >> output)) {
      // Default initializer, zero for primitive types.
      output = T_OUTPUT();
    }
    return output;
  }
};

template <typename T_INPUT, typename T_OUTPUT>
struct FromStringEnumImpl<T_INPUT, T_OUTPUT, true> {
  template <typename T>
  static const T_OUTPUT& Go(T&& input, T_OUTPUT& output) {
    std::istringstream is(input);
    using T_UNDERLYING_OUTPUT = typename std::underlying_type<T_OUTPUT>::type;
    T_UNDERLYING_OUTPUT underlying_output;
    if (!(is >> underlying_output)) {
      // Default initializer, zero for primitive types.
      underlying_output = T_UNDERLYING_OUTPUT();
    }
    output = static_cast<T_OUTPUT>(underlying_output);
    return output;
  }
};

template <typename T_OUTPUT, typename T_INPUT = std::string>
inline const T_OUTPUT& FromString(T_INPUT&& input, T_OUTPUT& output) {
  return FromStringEnumImpl<T_INPUT, T_OUTPUT, std::is_enum<T_OUTPUT>::value>::Go(std::forward<T_INPUT>(input),
                                                                                  output);
}

// Special case for `std::pair<>`. For Storage REST only for now.
// TODO(dkorolev) + TODO(mzhurovich): Unify `FromString()` and `JSON()` under one `Serialize()` w/ policies?
template <typename T_INPUT, typename T_OUTPUT_FST, typename T_OUTPUT_SND>
struct FromStringEnumImpl<T_INPUT, std::pair<T_OUTPUT_FST, T_OUTPUT_SND>, false> {
  static const std::pair<T_OUTPUT_FST, T_OUTPUT_SND>& Go(const std::string& input,
                                                         std::pair<T_OUTPUT_FST, T_OUTPUT_SND>& output) {
    const size_t dash = input.find('-');
    FromString(input.substr(0, dash), output.first);
    if (dash != std::string::npos) {
      FromString(input.substr(dash + 1), output.second);
    } else {
      output.second = T_OUTPUT_SND();
    }
    return output;
  }
};

template <typename T_INPUT>
inline const std::string& FromString(T_INPUT&& input, std::string& output) {
  output = input;
  return output;
}

template <typename T_INPUT>
inline bool FromString(T_INPUT&& input, bool& output) {
  output = (std::string("") != input && std::string("0") != input && std::string("false") != input);
  return output;
}

template <typename T_OUTPUT, typename T_INPUT = std::string>
inline T_OUTPUT FromString(T_INPUT&& input) {
  T_OUTPUT output;
  FromString(std::forward<T_INPUT>(input), output);
  return output;
}

inline std::string FromString(const std::string& input) { return input; }

template <size_t N>
constexpr ENABLE_IF<(N > 0), size_t> CompileTimeStringLength(char const(&)[N]) {
  return N - 1;
}

inline std::string Trim(const char* input, const size_t length) {
  const char* begin = input;
  const char* end = input + length;
  const char* output_begin = begin;
  while (output_begin < end && ::isspace(*output_begin)) {
    ++output_begin;
  }
  const char* output_end = end;
  while (output_end > output_begin && ::isspace(*(output_end - 1))) {
    --output_end;
  }
  return std::string(output_begin, output_end);
}

inline std::string Trim(const char* s) { return Trim(s, ::strlen(s)); }

inline std::string Trim(const std::string& s) { return Trim(s.c_str(), s.length()); }

template <typename T>
inline std::string ToLower(T begin, T end) {
  std::string result;
  std::transform(begin, end, std::back_inserter(result), ::tolower);
  return result;
}

inline std::string ToLower(const char* s) { return ToLower(s, s + ::strlen(s)); }

template <typename T>
inline std::string ToLower(const T& input) {
  return ToLower(std::begin(input), std::end(input));
}

template <typename T>
inline std::string ToUpper(T begin, T end) {
  std::string result;
  std::transform(begin, end, std::back_inserter(result), ::toupper);
  return result;
}

inline std::string ToUpper(const char* s) { return ToUpper(s, s + ::strlen(s)); }

template <typename T>
inline std::string ToUpper(const T& input) {
  return ToUpper(std::begin(input), std::end(input));
}

}  // namespace strings

using strings::ToString;
using strings::FromString;

}  // namespace current

#endif  // BRICKS_STRINGS_UTIL_H
