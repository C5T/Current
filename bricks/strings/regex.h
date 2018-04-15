/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2017 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_STRINGS_REGEX_H
#define BRICKS_STRINGS_REGEX_H

#include <cstring>
#include <regex>
#include <string>
#include <unordered_map>

#include "chunk.h"

namespace current {
namespace strings {

// Various ways to range-based-for-loop iterate over regex matches.
template <typename T>
struct IterateByRegexMatchesImpl;

template <>
struct IterateByRegexMatchesImpl<std::string&> final {
  using string_t = std::string&;
  using copy_free_string_t = const std::string&;
  using string_iterator_t = typename std::string::const_iterator;

  static string_iterator_t static_begin(copy_free_string_t s) { return s.begin(); }
  static string_iterator_t static_end(copy_free_string_t s) { return s.end(); }

  using iterator_t = std::sregex_iterator;
  using match_t = std::smatch;

  const iterator_t begin_;
  const iterator_t end_ = iterator_t();

  IterateByRegexMatchesImpl(const std::regex& re, std::string& s) : begin_(s.begin(), s.end(), re) {}

  std::sregex_iterator begin() const { return begin_; }
  std::sregex_iterator end() const { return end_; }
};

template <>
struct IterateByRegexMatchesImpl<const char*> final {
  using string_t = const char*;
  using copy_free_string_t = const char*;
  using string_iterator_t = const char*;

  static string_iterator_t static_begin(const char* s) { return s; }
  static string_iterator_t static_end(const char* s) { return s + std::strlen(s); }

  static string_iterator_t static_begin(Chunk s) { return s.c_str(); }
  static string_iterator_t static_end(Chunk s) { return s.c_str() + s.length(); }

  using iterator_t = std::cregex_iterator;
  using match_t = std::cmatch;

  const std::cregex_iterator begin_;
  const std::cregex_iterator end_;

  IterateByRegexMatchesImpl(const std::regex& re, const char* s)
      : begin_(s, s + std::strlen(s), re) {}

  IterateByRegexMatchesImpl(const std::regex& re, const char* begin, const char* end)
      : begin_(begin, end, re) {}

  std::cregex_iterator begin() const { return begin_; }
  std::cregex_iterator end() const { return end_; }
};

// The parsed and compiled regular expression annotated with group names.
struct AnnotatedRegex final {
  std::vector<std::string> group_names;
  std::unordered_map<std::string, size_t> group_indexes;
  std::string transformed_re_body;  // This should be long-lived too.
  std::regex transformed_re;

  AnnotatedRegex(std::string re_body) {
    // This regex requires explanation.
    // 1) Must begin with a non-escaped "(".
    // 2) Must not begin with "(?:" or "(?=".
    // 3) May, but does not have to be "(?<...>").
    const std::regex re_capture_groups("\\((?!\\?:)(?!\\?=)(\\?<(\\w+)>)?");
    for (const auto& capture_group : IterateByRegexMatchesImpl<std::string&>(re_capture_groups, re_body)) {
      if (!capture_group.position() || re_body[capture_group.position() - 1u] != '\\') {
        const std::string name = capture_group[2u].str();
        if (!name.empty()) {
          group_indexes[name] = group_names.size() + 1u;
        }
        group_names.push_back(name);
      }
    }
    transformed_re_body = std::regex_replace(re_body, re_capture_groups, "(");
    transformed_re = std::regex(transformed_re_body);
  }
};

// The real match. The `S` template parameter is `std::string` / `const char*` / `Chunk`.
template <typename S>
struct MatchResult final {
  static_assert(std::is_same<S, std::string&>::value ||
                std::is_same<S, const char*>::value ||
                std::is_same<S, Chunk>::value,
                "MatchResult<S> accepts  `std::string&` / `const char*` / `Chunk`.");

  using impl_t = IterateByRegexMatchesImpl<S>;

  using string_t = typename impl_t::string_t;
  using copy_free_string_t = typename impl_t::copy_free_string_t;
  using string_iterator_t = typename impl_t::string_iterator_t;

  using iterator_t = typename impl_t::iterator_t;
  using match_t = typename impl_t::match_t;

  std::shared_ptr<AnnotatedRegex> data;
  match_t match;

  MatchResult() = delete;

  MatchResult(copy_free_string_t s, std::shared_ptr<AnnotatedRegex> data)
      : data(data) {
    std::regex_match(impl_t::static_begin(s), impl_t::static_end(s), match, data->transformed_re);
  }

  MatchResult(string_iterator_t begin, string_iterator_t end, std::shared_ptr<AnnotatedRegex> data)
      : data(data) {
    std::regex_match(begin, end, match, data->transformed_re);
  }

  bool empty() const { return match.empty(); }
  size_t size() const { return match.size(); }
  size_t length() const { return match.length(); }
  size_t position() const { return match.position(); }

  template <typename SS>
  bool Has(SS&& s) const {
    const auto cit = data->group_indexes.find(std::forward<SS>(s));
    if (cit != data->group_indexes.end()) {
      const size_t i = cit->second;
      if (i < match.size()) {
        return match[i].length() > 0u;
      }
    }
    return false;
  }

  template <typename SS>
  std::string Get(SS&& s, std::string default_value = "") const {
    const auto cit = data->group_indexes.find(std::forward<SS>(s));
    if (cit != data->group_indexes.end()) {
      const size_t i = cit->second;
      if (i < match.size()) {
        return match[i].str();
      }
    }
    return default_value;
  }

  template <typename SS>
  std::string operator[](SS&& s) const {
    return Get(std::forward<SS>(s));
  }
};

// `Iterable` and `Iterator`.
template <typename S>
struct Iterable final {
  static_assert(std::is_same<S, std::string&>::value ||
                std::is_same<S, const char*>::value ||
                std::is_same<S, Chunk>::value,
                "Iterable<S> accepts  `std::string&` / `const char*` / `Chunk`.");

  using impl_t = IterateByRegexMatchesImpl<S>;

  using string_t = typename impl_t::string_t;
  using copy_free_string_t = typename impl_t::copy_free_string_t;
  using string_iterator_t = typename impl_t::string_iterator_t;

  using iterator_t = typename impl_t::iterator_t;
  using match_t = typename impl_t::match_t;

  const std::shared_ptr<AnnotatedRegex> data_;
  const iterator_t begin_;

  Iterable(std::shared_ptr<AnnotatedRegex> data, copy_free_string_t s)
      : data_(data),
        begin_(impl_t::static_begin(s), impl_t::static_end(s), data_->transformed_re) {
  }

  Iterable(std::shared_ptr<AnnotatedRegex> data, string_iterator_t begin, string_iterator_t end)
      : data_(data),
        begin_(begin, end, data_->transformed_re) {
  }

  struct Iterator final {
    const std::shared_ptr<AnnotatedRegex> data_shared_ptr_;
    const AnnotatedRegex& data_;
    iterator_t iterator_;
    Iterator(std::shared_ptr<AnnotatedRegex> data, iterator_t iterator)
        : data_shared_ptr_(data), data_(*data_shared_ptr_), iterator_(iterator) {}
    void operator++() { ++iterator_; }
    bool operator==(const Iterator& rhs) const { return iterator_ == rhs.iterator_; }  // No need to compare `data_.`
    bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }
    struct Accessor {
      const AnnotatedRegex& data;
      match_t match;
      Accessor(const AnnotatedRegex& data, match_t match) : data(data), match(match) {}
      bool empty() const { return match.empty(); }
      size_t size() const { return match.size(); }
      size_t length() const { return match.length(); }
      size_t position() const { return match.position(); }
      std::string str() const { return match.str(); }
      const match_t& smatch() const { return match; }
      template <typename SS>
      bool Has(SS&& s) const {
        const auto cit = data.group_indexes.find(std::forward<SS>(s));
        if (cit != data.group_indexes.end()) {
          const size_t i = cit->second;
          if (i < match.size()) {
            return match[i].length() > 0u;
          }
        }
        return false;
      }
      template <typename SS>
      std::string Get(SS&& s, std::string default_value = "") const {
        const auto cit = data.group_indexes.find(std::forward<SS>(s));
        if (cit != data.group_indexes.end()) {
          const size_t i = cit->second;
          if (i < match.size()) {
            return match[i].str();
          }
        }
        return default_value;
      }
      template <typename SS>
      std::string operator[](SS&& s) const {
        return Get(std::forward<SS>(s));
      }
    };
    Accessor operator*() const { return Accessor(data_, *iterator_); }
  };
  Iterator begin() const { return Iterator(data_, begin_); }
  Iterator end() const { return Iterator(data_, iterator_t());  }  // end_); }
};


// Regexes in C++ don't support named capture. In Current, we have a workaround. Refresher:
// 1) `(blah)` is an unnamed capture group (referencable to by its index in the regex though).
// 2) `(?:blah)` is a non-capturing group (should match, but can't be referred to).
// 3) `(?<whoa>blah)` is a named capture -- the one C++ does not support -- allowing referring to "blah" as `whoa`.
// 4) Obviously, `\(` is an opening parenthesis, not the beginning of the group.
// template <typename S>
class NamedRegexCapturer final {
 private:
  std::shared_ptr<AnnotatedRegex> data_;

 public:
  // As the regex body will have to be copied over as an `std::string`, there's no harm
  // in accepting it as an `std::string` by copy and `std::move()`-ing it into `AnnotatedRegex`-s constructor.
  explicit NamedRegexCapturer(std::string re_body) : data_(std::make_shared<AnnotatedRegex>(std::move(re_body))) {}

  // Tests, matches, and iterations.
  bool Test(const std::string& s) const {
    // NOTE: For `Test()` (no iteration), accepting a string by const reference
    // is perfectly acceptable, as it does not have to outlive the very call.
    return std::regex_match(s, data_->transformed_re);
  }

  bool Test(const char* s) const {
    return std::regex_match(s, data_->transformed_re);
  }

  bool Test(Chunk s) const {
    return std::regex_match(s.c_str(), s.c_str() + s.length(), data_->transformed_re);
  }

  MatchResult<std::string&> Match(std::string& s) const {
    // NOTE: The `s` should be long-lived in this call, hence it's accepted as a mutable `std::string&`.
    return MatchResult<std::string&>(s, data_);
  }

  MatchResult<std::string&> MatchLongLivedString(const std::string& s) const {
    // NOTE: The name is deliberately longer to avoid the possiblity of using
    // a temporary string later, as it is out of scope.
    return MatchResult<std::string&>(const_cast<std::string&>(s), data_);
  }

  MatchResult<const char*> Match(const char* s) const {
    return MatchResult<const char*>(s, data_);
  }

  MatchResult<const char*> Match(Chunk s) const {
    return MatchResult<const char*>(s.c_str(), s.c_str() + s.length(), data_);
  }

  Iterable<std::string&> Iterate(std::string& s) const {
    // NOTE: The `s` should be long-lived in this call, hence it's accepted as a mutable `std::string&`.
    return Iterable<std::string&>(data_, s);
  }

  Iterable<std::string&> IterateOverLongLivedString(const std::string& s) const {
    // NOTE: The name is deliberately longer to avoid the possiblity of using
    // a temporary string later, as it is out of scope.
    return Iterable<std::string&>(data_, const_cast<std::string&>(s));
  }

  Iterable<std::string&> Iterate(typename std::string::const_iterator begin,
                                 typename std::string::const_iterator end) const {
    return Iterable<std::string&>(data_, begin, end);
  }

  Iterable<const char*> Iterate(const char* s) const {
    return Iterable<const char*>(data_, s);
  }

  Iterable<const char*> Iterate(const char* begin, const char* end) const {
    return Iterable<const char*>(data_, begin, end);
  }

  Iterable<const char*> Iterate(Chunk s) const {
    return Iterable<const char*>(data_, s.c_str(), s.c_str() + s.length());
  }

  // Various getters.
  size_t TotalCaptures() const { return data_->group_names.size(); }
  size_t NamedCaptures() const { return data_->group_indexes.size(); }
  const std::string& GetTransformedRegexBody() const { return data_->transformed_re_body; }
  const std::regex& GetTransformedRegex() const { return data_->transformed_re; }
  const std::vector<std::string>& GetTransformedRegexCaptureGroupNames() const { return data_->group_names; }
  const std::unordered_map<std::string, size_t>& GetTransformedRegexCaptureGroupIndexes() const {
    return data_->group_indexes;
  }
};

inline IterateByRegexMatchesImpl<std::string&> IterateByRegexMatches(const std::regex& re, std::string& s) {
  return IterateByRegexMatchesImpl<std::string&>(re, s);
}

inline IterateByRegexMatchesImpl<std::string&> IterateByRegexMatchesOverLongLivedString(
    const std::regex& re, 
    const std::string& s) {
  return IterateByRegexMatchesImpl<std::string&>(re, const_cast<std::string&>(s));
}

inline IterateByRegexMatchesImpl<const char*> IterateByRegexMatches(const std::regex& re, const char* s) {
  return IterateByRegexMatchesImpl<const char*>(re, s);
}

inline IterateByRegexMatchesImpl<const char*> IterateByRegexMatches(const std::regex& re, Chunk s) {
  return IterateByRegexMatchesImpl<const char*>(re, s.c_str(), s.c_str() + s.length());
}

}  // namespace strings
}  // namespace current

#endif  // BRICKS_STRINGS_ESCAPE_H
