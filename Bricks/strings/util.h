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

#include "../template/decay.h"

namespace bricks {
namespace strings {

template <typename DECAYED_T>
struct ToStringImpl {
  static typename std::enable_if<std::is_pod<DECAYED_T>::value, std::string>::type ToString(DECAYED_T value) {
    return std::to_string(value);
  }
};

template <>
struct ToStringImpl<std::string> {
  template <typename T>
  static std::string ToString(T&& string) {
    return string;
  }
};

template <>
struct ToStringImpl<char*> {  // Decayed type in template parameters list.
  static std::string ToString(const char* string) { return string; }
};

template <int N>
struct ToStringImpl<char[N]> {  // Decayed type in template parameters list.
  static std::string ToString(const char string[N]) {
    return std::string(string, string + N - 1);  // Do not include the '\0' character.
  }
};

template <>
struct ToStringImpl<char> {
  static std::string ToString(char c) { return std::string(1u, c); }
};

// Keep the lowercase name to possibly act as a replacement for `std::to_string`.
template <typename T>
inline std::string to_string(T&& something) {
  return ToStringImpl<bricks::decay<T>>::ToString(something);
}

// Use camel-case `ToString()` within Bricks.
template <typename T>
inline std::string ToString(T&& something) {
  return bricks::strings::to_string(something);
}

template <typename T_OUTPUT, typename T_INPUT = std::string>
inline const T_OUTPUT& FromString(T_INPUT&& input, T_OUTPUT& output) {
  std::istringstream is(input);
  if (!(is >> output)) {
    // Default initializer, zero for primitive types.
    output = T_OUTPUT();
  }
  return output;
}

template <typename T_OUTPUT, typename T_INPUT = std::string>
inline T_OUTPUT FromString(T_INPUT&& input) {
  T_OUTPUT output;
  FromString(std::forward<T_INPUT>(input), output);
  return output;
}

template <size_t N>
constexpr typename std::enable_if<(N > 0), size_t>::type CompileTimeStringLength(char const (&)[N]) {
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
}  // namespace bricks

#endif  // BRICKS_STRINGS_UTIL_H
