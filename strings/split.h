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
#include <vector>
#include <string>
#include <type_traits>

namespace bricks {
namespace strings {

namespace impl {

template <typename T>
struct MatchImpl {};

template <>
struct MatchImpl<char> {
  inline static bool Match(char a, char b) { return a == b; }
};

template <>
struct MatchImpl<std::string> {
  inline static bool Match(char a, const std::string& b) { return b.find(a) != std::string::npos; }
};

template <size_t N>
struct MatchImpl<const char[N]> {
  static typename std::enable_if<(N > 0), bool>::type Match(char a, const char b[N]) {
    return std::find(b, b + N, a) != (b + N);
  }
};

template <typename T>
inline bool Match(char a, T&& b) {
  return MatchImpl<typename std::remove_reference<T>::type>::Match(a, b);
}

}  // namespace impl

enum class TrimMode { Trim, NoTrim };

template <typename T_SEPARATOR, typename T_PROCESSOR>
size_t Split(const std::string& s,
             T_SEPARATOR&& separator,
             T_PROCESSOR&& processor,
             TrimMode trim = TrimMode::Trim) {
  size_t i = 0;
  size_t j = 0;
  size_t n = 0;
  const auto emit = [&]() {
    if (trim == TrimMode::NoTrim || i != j) {
      ++n;
      processor(s.substr(j, i - j));
    }
  };
  for (i = 0; i < s.size(); ++i) {
    if (impl::Match(s[i], separator)) {
      emit();
      j = i + 1;
    }
  }
  emit();
  return n;
}

template <typename T_SEPARATOR>
inline std::vector<std::string> Split(const std::string& s,
                                      T_SEPARATOR&& separator,
                                      TrimMode trim = TrimMode::Trim) {
  std::vector<std::string> result;
  Split(s, separator, [&result](std::string&& chunk) { result.emplace_back(std::move(chunk)); }, trim);
  return result;
}

}  // namespace strings
}  // namespace bricks

#endif  // BRICKS_STRINGS_SPLIT_H
