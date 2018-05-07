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

#include "chunk.h"

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
constexpr auto HasMemberFromString(int) -> decltype(std::declval<T>().FromString(std::declval<const char*>()), bool()) {
  return true;
}

}  // namespace sfinae

// Default implepentation for arithmetic types calling `std::to_string`.
template <typename DECAYED_T, bool HAS_MEMBER_TO_STRING, bool IS_ENUM>
struct ToStringImpl {
  template <bool B = std::is_arithmetic<DECAYED_T>::value>
  static ENABLE_IF<B, std::string> DoIt(DECAYED_T value) {
    return std::to_string(value);
  }
};

// `bool`.
template <>
struct ToStringImpl<bool, false, false> {
  static std::string DoIt(bool b) { return b ? "true" : "false"; }
};

// Types that implement the `ToString()` method.
template <typename DECAYED_T>
struct ToStringImpl<DECAYED_T, true, false> {
  static std::string DoIt(const DECAYED_T& x) { return x.ToString(); }
};

// Enums.
template <typename DECAYED_T>
struct ToStringImpl<DECAYED_T, false, true> {
  static std::string DoIt(DECAYED_T value) {
    return std::to_string(static_cast<typename std::underlying_type<DECAYED_T>::type>(value));
  }
};

// `std::string`.
template <>
struct ToStringImpl<std::string, false, false> {
  static std::string DoIt(const std::string& s) { return s; }
};

// `const char*`.
template <>
struct ToStringImpl<const char*, false, false> {
  static std::string DoIt(const char* string) { return string; }
};

// `const char[]`.
template <int N>
struct ToStringImpl<char[N], false, false> {
  static std::string DoIt(const char string[N]) {
    return std::string(string, string + N - 1);  // Do not include the '\0' character.
  }
};
template <int N>
struct ToStringImpl<const char[N], false, false> {
  static std::string DoIt(const char string[N]) {
    return std::string(string, string + N - 1);  // Do not include the '\0' character.
  }
};

// `char`.
template <>
struct ToStringImpl<char, false, false> {
  static std::string DoIt(char c) { return std::string(1u, c); }
};

// `std::chrono::milliseconds`.
template <>
struct ToStringImpl<std::chrono::milliseconds, false, false> {
  static std::string DoIt(std::chrono::milliseconds t) { return std::to_string(t.count()); }
};

// `std::chrono::microseconds`.
template <>
struct ToStringImpl<std::chrono::microseconds, false, false> {
  static std::string DoIt(std::chrono::microseconds t) { return std::to_string(t.count()); }
};

// `Chunk` uses its own native cast into `std::string`, but it should be enabled.
template <>
struct ToStringImpl<strings::Chunk, false, false> {
  static std::string DoIt(strings::Chunk chunk) { return chunk; }
};

template <typename T>
inline std::string ToString(T&& something) {
  using decayed_t = current::decay<T>;
  return ToStringImpl<decayed_t, sfinae::HasMemberToString<decayed_t>(0), std::is_enum<decayed_t>::value>::DoIt(
      std::forward<T>(something));
}

template <typename INPUT, typename OUTPUT, bool HAS_MEMBER_FROM_STRING, bool IS_ENUM>
struct FromStringImpl;

template <typename INPUT, typename OUTPUT>
struct FromStringImpl<INPUT, OUTPUT, true, false> {
  static const OUTPUT& Go(INPUT&& input, OUTPUT& output) {
    output.FromString(std::forward<INPUT>(input));
    return output;
  }
};

template <typename INPUT, typename OUTPUT>
struct FromStringImpl<INPUT, OUTPUT, false, false> {
  static const OUTPUT& Go(INPUT&& input, OUTPUT& output) {
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
  static const OUTPUT& Go(INPUT&& input, OUTPUT& output) {
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
struct FromStringImpl<INPUT, bool, false, false> {
  // Must return a reference as the callers expects it so. -- D.K.
  static const bool& Go(const std::string& input, bool& output) {
    output = !input.empty() && input != "0" && input != "false" && input != "False" && input != "FALSE";
    return output;
  }
};

// Well, `int8_t` and `uint8_t` are not f*cking `char`-s, contrary to what `std::istringstream` thinks they are.
template <typename INPUT>
struct FromStringImpl<INPUT, int8_t, false, false> {
  // Must return a reference as the callers expects it so. -- D.K.
  static const int8_t& Go(const std::string& input, int8_t& output) {
    std::istringstream is(input);
    int value;
    is >> value;
    output = static_cast<int8_t>(value);
    return output;
  }
};
template <typename INPUT>
struct FromStringImpl<INPUT, uint8_t, false, false> {
  // Must return a reference as the callers expects it so. -- D.K.
  static const uint8_t& Go(const std::string& input, uint8_t& output) {
    std::istringstream is(input);
    int value;
    is >> value;
    output = static_cast<uint8_t>(value);
    return output;
  }
};


template <typename INPUT>
struct FromStringImpl<INPUT, std::chrono::milliseconds, false, false> {
  static const std::chrono::milliseconds& Go(INPUT&& input, std::chrono::milliseconds& output) {
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
  static const std::chrono::microseconds& Go(INPUT&& input, std::chrono::microseconds& output) {
    std::istringstream is(input);
    int64_t underlying_output;
    if (!(is >> underlying_output)) {
      underlying_output = 0ll;
    }
    output = static_cast<std::chrono::microseconds>(underlying_output);
    return output;
  }
};

template <typename INPUT>
inline const std::string& FromString(INPUT&& input, std::string& output) {
  output = input;
  return output;
}

template <typename OUTPUT, typename INPUT = std::string>
inline const OUTPUT& FromString(INPUT&& input, OUTPUT& output) {
  return FromStringImpl<INPUT, OUTPUT, sfinae::HasMemberFromString<OUTPUT>(0), std::is_enum<OUTPUT>::value>::Go(
      std::forward<INPUT>(input), output);
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

inline size_t UTF8StringLength(char const* s) {
  size_t length = 0u;
  while (*s) {
    if ((*s++ & 0xc0) != 0x80) {
      ++length;
    }
  }
  return length;
}

inline size_t UTF8StringLength(std::string const& s) { return UTF8StringLength(s.c_str()); }

// Another helper wrapper for `.c_str()`-using code to be able to accept `template<typename S>`.
inline const char* ConstCharPtr(const char* s) { return s; }
inline const char* ConstCharPtr(Chunk c) { return c.c_str(); }
inline const char* ConstCharPtr(const std::string& s) { return s.c_str(); }

}  // namespace strings

using strings::ToString;
using strings::FromString;

}  // namespace current

#endif  // BRICKS_STRINGS_UTIL_H
