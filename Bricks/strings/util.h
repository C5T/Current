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
#include <chrono>
#include <cstring>
#include <sstream>
#include <string>

#include "is_string_type.h"

#include "../template/enable_if.h"
#include "../template/decay.h"

namespace current {
namespace strings {

namespace sfinae {

template <typename T>
constexpr bool HasMemberToString(char) {
  return false;
}

template <typename T>
constexpr auto HasMemberToString(int) -> decltype(std::declval<const T>().ToString(), bool()) {
  return true;
}

// TODO(dkorolev) + TODO(mzhurovich): Perhaps `C(ConstructFromString, s)`, not member `C.FromString(s)`?
template <typename T>
constexpr bool HasMemberFromString(char) {
  return false;
}

template <typename T>
constexpr auto HasMemberFromString(int) -> decltype(std::declval<T>().FromString(""), bool()) {
  return true;
}

}  // namespace sfinae

template <typename DECAYED_T, bool HAS_MEMBER_TO_STRING, bool IS_ENUM>
struct ToStringImpl {
  template <bool B = std::is_pod<DECAYED_T>::value>
  static ENABLE_IF<B, std::string> DoIt(DECAYED_T value) {
    return std::to_string(value);
  }
};

template <typename DECAYED_T>
struct ToStringImpl<DECAYED_T, false, true> {
  template <bool B = std::is_pod<DECAYED_T>::value>
  static ENABLE_IF<B, std::string> DoIt(DECAYED_T value) {
    return std::to_string(static_cast<typename std::underlying_type<DECAYED_T>::type>(value));
  }
};

template <typename DECAYED_T, bool B>
struct ToStringImpl<DECAYED_T, true, B> {
  template <typename T>
  static std::string DoIt(T&& x) {
    return x.ToString();
  }
};

template <>
struct ToStringImpl<std::string, false, false> {
  template <typename T>
  static std::string DoIt(T&& string) {
    return string;
  }
};

template <>
struct ToStringImpl<std::chrono::milliseconds, false, false> {
  static std::string DoIt(std::chrono::milliseconds t) { return std::to_string(t.count()); }
};

template <>
struct ToStringImpl<std::chrono::microseconds, false, false> {
  static std::string DoIt(std::chrono::microseconds t) { return std::to_string(t.count()); }
};

template <>
struct ToStringImpl<char*, false, false> {  // Decayed type in template parameters list.
  static std::string DoIt(const char* string) { return string; }
};

template <int N>
struct ToStringImpl<char[N], false, false> {  // Decayed type in template parameters list.
  static std::string DoIt(const char string[N]) {
    return std::string(string, string + N - 1);  // Do not include the '\0' character.
  }
};

template <>
struct ToStringImpl<char, false, false> {
  static std::string DoIt(char c) { return std::string(1u, c); }
};

template <>
struct ToStringImpl<bool, false, false> {
  static std::string DoIt(bool b) { return b ? "true" : "false"; }
};

template <typename T>
inline std::string ToString(T&& something) {
  using DECAYED_T = current::decay<T>;
  return ToStringImpl<DECAYED_T, sfinae::HasMemberToString<T>(0), std::is_enum<DECAYED_T>::value>::DoIt(
      something);
}

// Special case for `std::pair<>`. For Storage REST only for now.
// TODO(dkorolev) + TODO(mzhurovich): Unify `ToString()` and `JSON()` under one `Serialize()` w/ policies?
template <typename FST, typename SND>
struct ToStringImpl<std::pair<FST, SND>, false, false> {
  static std::string DoIt(const std::pair<FST, SND>& pair) {
    return ToString(pair.first) + ':' + ToString(pair.second);
  }
};

template <typename INPUT, typename OUTPUT, bool HAS_MEMBER_FROM_STRING, bool IS_ENUM>
struct FromStringImpl;

template <typename INPUT, typename OUTPUT>
struct FromStringImpl<INPUT, OUTPUT, true, false> {
  template <typename T>
  static const OUTPUT& Go(T&& input, OUTPUT& output) {
    output.FromString(std::forward<T>(input));
    return output;
  }
};

template <typename INPUT, typename OUTPUT>
struct FromStringImpl<INPUT, OUTPUT, false, false> {
  template <typename T>
  static const OUTPUT& Go(T&& input, OUTPUT& output) {
    std::istringstream is(input);
    if (!(is >> output)) {
      // Default initializer, zero for primitive types.
      output = OUTPUT();
    }
    return output;
  }
};

template <typename INPUT, typename OUTPUT>
struct FromStringImpl<INPUT, OUTPUT, false, true> {
  template <typename T>
  static const OUTPUT& Go(T&& input, OUTPUT& output) {
    std::istringstream is(input);
    using underlying_output_t = typename std::underlying_type<OUTPUT>::type;
    underlying_output_t underlying_output;
    if (!(is >> underlying_output)) {
      // Default initializer, zero for primitive types.
      underlying_output = underlying_output_t();
    }
    output = static_cast<OUTPUT>(underlying_output);
    return output;
  }
};

template <typename INPUT>
struct FromStringImpl<INPUT, std::chrono::milliseconds, false, false> {
  template <typename T>
  static const std::chrono::milliseconds& Go(T&& input, std::chrono::milliseconds& output) {
    std::istringstream is(input);
    int64_t underlying_output;
    if (!(is >> underlying_output)) {
      underlying_output = 0ll;
    }
    output = static_cast<std::chrono::milliseconds>(underlying_output);
    return output;
  }
};

template <typename INPUT>
struct FromStringImpl<INPUT, std::chrono::microseconds, false, false> {
  template <typename T>
  static const std::chrono::microseconds& Go(T&& input, std::chrono::microseconds& output) {
    std::istringstream is(input);
    int64_t underlying_output;
    if (!(is >> underlying_output)) {
      underlying_output = 0ll;
    }
    output = static_cast<std::chrono::microseconds>(underlying_output);
    return output;
  }
};

template <typename OUTPUT, typename INPUT = std::string>
inline const OUTPUT& FromString(INPUT&& input, OUTPUT& output) {
  return FromStringImpl<INPUT, OUTPUT, sfinae::HasMemberFromString<OUTPUT>(0), std::is_enum<OUTPUT>::value>::Go(
      std::forward<INPUT>(input), output);
}

// Special case for `std::pair<>`. For Storage REST only for now.
// TODO(dkorolev) + TODO(mzhurovich): Unify `FromString()` and `JSON()` under one `Serialize()` w/ policies?
template <typename INPUT, typename OUTPUT_FST, typename OUTPUT_SND>
struct FromStringImpl<INPUT, std::pair<OUTPUT_FST, OUTPUT_SND>, false, false> {
  static const std::pair<OUTPUT_FST, OUTPUT_SND>& Go(const std::string& input,
                                                     std::pair<OUTPUT_FST, OUTPUT_SND>& output) {
    const size_t dash = input.find('-');
    FromString(input.substr(0, dash), output.first);
    if (dash != std::string::npos) {
      FromString(input.substr(dash + 1), output.second);
    } else {
      output.second = OUTPUT_SND();
    }
    return output;
  }
};

template <typename INPUT>
inline const std::string& FromString(INPUT&& input, std::string& output) {
  output = input;
  return output;
}

template <typename INPUT>
inline bool FromString(INPUT&& input, bool& output) {
  output = (std::string("") != input && std::string("0") != input && std::string("false") != input);
  return output;
}

template <typename OUTPUT, typename INPUT = std::string>
inline OUTPUT FromString(INPUT&& input) {
  OUTPUT output;
  FromString(std::forward<INPUT>(input), output);
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
