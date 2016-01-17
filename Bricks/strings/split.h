/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_STRINGS_SPLIT_H
#define BRICKS_STRINGS_SPLIT_H

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

#include "chunk.h"

#include "../exception.h"
#include "../template/enable_if.h"
#include "../template/decay.h"
#include "../template/weed.h"

namespace current {
namespace strings {

// Skip: Skip empty chunks, Keep: Keep empty fields.
enum class EmptyFields { Skip, Keep };

// Silent:  Silently ignore the key-value pairs that can not be parsed.
// Throw:   Throw an exception if the input string is malfomed.
enum class KeyValueParsing { Silent, Throw };
enum class ByWhitespace { UseIsSpace };
enum class ByLines { Use0Aor0D };

struct KeyValueNoValueException : Exception {};
struct KeyValueMultipleValuesException : Exception {};

namespace impl {

template <typename T>
struct MatchImpl {
  enum { valid_separator = false };
};

template <>
struct MatchImpl<char> {
  enum { valid_separator = true };
  inline static bool Match(char a, char b) { return a == b; }
};

template <>
struct MatchImpl<ByWhitespace> {
  enum { valid_separator = true };
  inline static bool Match(char c, ByWhitespace) { return !!(::isspace(c)); }
};

template <>
struct MatchImpl<ByLines> {
  enum { valid_separator = true };
  inline static bool Match(char c, ByLines) { return c == '\n' || c == '\r'; }
};

template <>
struct MatchImpl<std::string> {
  enum { valid_separator = true };
  inline static bool Match(char c, const std::string& s) { return s.find(c) != std::string::npos; }
};

template <size_t N>
struct MatchImpl<const char[N]> {
  enum { valid_separator = true };
  static ENABLE_IF<(N > 0), bool> Match(char c, const char s[N]) { return std::find(s, s + N, c) != (s + N); }
};

template <typename T>
inline ENABLE_IF<!weed::call_with<T, char>::implemented, bool> Match(char a, T&& b) {
  return MatchImpl<rmref<T>>::Match(a, std::forward<T>(b));
}

template <typename T>
inline ENABLE_IF<weed::call_with<T, char>::implemented, bool> Match(char a, T&& b) {
  return !b(a);
}

template <bool B, typename T>
struct IsValidSeparatorImpl {
  enum { value = true };
};

template <typename T>
struct IsValidSeparatorImpl<false, T> {
  enum { value = weed::call_with<T, char>::implemented };
};

template <typename T>
struct IsValidSeparator : IsValidSeparatorImpl<MatchImpl<rmref<T>>::valid_separator, rmref<T>> {};

template <typename T>
struct DefaultSeparator {};

template <>
struct DefaultSeparator<ByWhitespace> {
  static inline ByWhitespace value() { return ByWhitespace::UseIsSpace; }
};

template <>
struct DefaultSeparator<ByLines> {
  static inline ByLines value() { return ByLines::Use0Aor0D; }
};

}  // namespace impl

template <typename T_SEPARATOR, typename T_PROCESSOR>
inline size_t Split(const std::string& s,
                    T_SEPARATOR&& separator,
                    T_PROCESSOR&& processor,
                    EmptyFields empty_fields_strategy = EmptyFields::Skip) {
  size_t i = 0;
  size_t j = 0;
  size_t n = 0;
  const auto emit = [&]() {
    if (empty_fields_strategy == EmptyFields::Keep || i != j) {
      ++n;
      processor(s.substr(j, i - j));
    }
  };
  for (i = 0; i < s.size(); ++i) {
    if (impl::Match(s[i], std::forward<T_SEPARATOR>(separator))) {
      emit();
      j = i + 1;
    }
  }
  emit();
  return n;
}

// A performant version accepting a mutable string, modifying it in-place, and emitting `Chunk`-s.
// NOTE: The user should not save intermediate Chunk-s, they are only valid throughout the call to the lambda.
//       (As soon as the call returns, the '\0' temporary injected into the string will no longer be there.)
template <typename T_SEPARATOR, typename T_PROCESSOR>
inline size_t Split(char* s,
                    const size_t length,
                    T_SEPARATOR&& separator,
                    T_PROCESSOR&& processor,
                    EmptyFields empty_fields_strategy = EmptyFields::Skip) {
  size_t i = 0;
  size_t j = 0;
  size_t n = 0;
  const auto emit = [&]() {
    if (empty_fields_strategy == EmptyFields::Keep || i != j) {
      ++n;
      const auto save = s[i];
      s[i] = '\0';
      processor(Chunk(&s[j], i - j));
      s[i] = save;
    }
  };
  for (i = 0; i < length; ++i) {
    if (impl::Match(s[i], std::forward<T_SEPARATOR>(separator))) {
      emit();
      j = i + 1;
    }
  }
  emit();
  return n;
}

// `Split(std::string&, ...)` maps to `Split(char*, size_t, ...)`.
template <typename T_SEPARATOR, typename T_PROCESSOR>
inline size_t Split(std::string& string,
                    T_SEPARATOR&& separator,
                    T_PROCESSOR&& processor,
                    EmptyFields empty_fields_strategy = EmptyFields::Skip) {
  return Split<T_SEPARATOR, T_PROCESSOR>(&string[0],
                                         string.length(),
                                         std::forward<T_SEPARATOR>(separator),
                                         std::forward<T_PROCESSOR>(processor),
                                         empty_fields_strategy);
}

// Be careful with top-level `char*` and `const char*` implementations.
// There's an issue with `gcc` attepmting to pass an "immutable string" as `char*`, not `const char*`, which
// first emits a `deprecated conversion from string constant to ‘char*’` warning, and then breaks everything.
template <typename T_SEPARATOR, typename T_PROCESSOR>
inline size_t Split(char* string,
                    T_SEPARATOR&& separator,
                    T_PROCESSOR&& processor,
                    EmptyFields empty_fields_strategy = EmptyFields::Skip) {
  return Split<T_SEPARATOR, T_PROCESSOR>(string,
                                         ::strlen(string),
                                         std::forward<T_SEPARATOR>(separator),
                                         std::forward<T_PROCESSOR>(processor),
                                         empty_fields_strategy);
}

template <typename T_SEPARATOR, typename T_PROCESSOR>
inline ENABLE_IF<!std::is_same<T_PROCESSOR, EmptyFields>::value, size_t> Split(
    const char* string,
    T_SEPARATOR&& separator,
    T_PROCESSOR&& processor,
    EmptyFields empty_fields_strategy = EmptyFields::Skip) {
  return Split<T_SEPARATOR, T_PROCESSOR>(std::string(string),
                                         std::forward<T_SEPARATOR>(separator),
                                         std::forward<T_PROCESSOR>(processor),
                                         empty_fields_strategy);
}

// Special case for `Chunk`: Treat even the `const` Chunk-s as mutable.
// We assume the users of `Chunk` a) expect performance, and b) know what they are doing.
template <typename T_SEPARATOR, typename T_PROCESSOR>
inline size_t Split(const Chunk& chunk,
                    T_SEPARATOR&& separator,
                    T_PROCESSOR&& processor,
                    EmptyFields empty_fields_strategy = EmptyFields::Skip) {
  return Split<T_SEPARATOR, T_PROCESSOR>(const_cast<char*>(chunk.c_str()),
                                         chunk.length(),
                                         std::forward<T_SEPARATOR>(separator),
                                         std::forward<T_PROCESSOR>(processor),
                                         empty_fields_strategy);
}

// Allow the `Split<ByWhitespace/ByLines>(string, processor [, empty_fields_strategy]);` synopsis.
template <typename T_SEPARATOR, typename T_PROCESSOR, typename T_STRING>
inline size_t Split(T_STRING&& s,
                    T_PROCESSOR&& processor,
                    EmptyFields empty_fields_strategy = EmptyFields::Skip) {
  return Split(std::forward<T_STRING>(s),
               impl::DefaultSeparator<T_SEPARATOR>::value(),
               std::forward<T_PROCESSOR>(processor),
               empty_fields_strategy);
}

// The versions returning an `std::vector<std::string>`, for those not caring about performance.
template <typename T_SEPARATOR, typename T_STRING>
inline ENABLE_IF<impl::IsValidSeparator<T_SEPARATOR>::value, std::vector<std::string>> Split(
    T_STRING&& s,
    T_SEPARATOR&& separator = impl::DefaultSeparator<T_SEPARATOR>::value(),
    EmptyFields empty_fields_strategy = EmptyFields::Skip) {
  std::vector<std::string> result;
  Split(std::forward<T_STRING>(s),
        std::forward<T_SEPARATOR>(separator),
        [&result](std::string&& chunk) { result.emplace_back(std::move(chunk)); },
        empty_fields_strategy);
  return result;
}

template <typename T_KEY_VALUE_SEPARATOR, typename T_FIELDS_SEPARATOR, typename T_STRING>
inline std::vector<std::pair<std::string, std::string>> SplitIntoKeyValuePairs(
    T_STRING&& s,
    T_KEY_VALUE_SEPARATOR&& key_value_separator,
    T_FIELDS_SEPARATOR&& fields_separator = impl::DefaultSeparator<T_FIELDS_SEPARATOR>::value(),
    KeyValueParsing throw_mode = KeyValueParsing::Silent) {
  std::vector<std::pair<std::string, std::string>> result;
  Split(std::forward<T_STRING>(s),
        std::forward<T_FIELDS_SEPARATOR>(fields_separator),
        [&result, &key_value_separator, &throw_mode](std::string&& key_and_value_as_one_string) {
          const std::vector<std::string> key_and_value =
              Split(key_and_value_as_one_string, std::forward<T_KEY_VALUE_SEPARATOR>(key_value_separator));
          if (key_and_value.size() >= 2) {
            if (key_and_value.size() == 2) {
              result.emplace_back(key_and_value[0], key_and_value[1]);
            } else if (throw_mode == KeyValueParsing::Throw) {
              CURRENT_THROW(KeyValueMultipleValuesException());
            }
          } else {
            if (throw_mode == KeyValueParsing::Throw) {
              CURRENT_THROW(KeyValueNoValueException());
            }
          }
        });
  return result;
}

template <typename T_KEY_VALUE_SEPARATOR, typename T_STRING>
inline std::vector<std::pair<std::string, std::string>> SplitIntoKeyValuePairs(
    T_STRING&& s,
    T_KEY_VALUE_SEPARATOR&& key_value_separator,
    KeyValueParsing throw_mode = KeyValueParsing::Silent) {
  return SplitIntoKeyValuePairs(
      std::forward<T_STRING>(s), key_value_separator, ByWhitespace::UseIsSpace, throw_mode);
}

}  // namespace strings
}  // namespace current

#endif  // BRICKS_STRINGS_SPLIT_H
