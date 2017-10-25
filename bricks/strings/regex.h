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

#include <regex>
#include <string>
#include <unordered_map>

namespace current {
namespace strings {

// A simple way to range-based-for-loop iterate over regex matches.
struct IterateByRegexMatches {
  const std::string copy_of_string_if_needed_;
  const std::sregex_iterator begin_;
  const std::sregex_iterator end_ = std::sregex_iterator();

  IterateByRegexMatches(const std::regex& re, const char* s)
      : copy_of_string_if_needed_(s), begin_(copy_of_string_if_needed_.begin(), copy_of_string_if_needed_.end(), re) {}

  template <typename T>
  IterateByRegexMatches(const std::regex& re, T&& s)
      : begin_(s.begin(), s.end(), re) {}

  std::sregex_iterator begin() const { return begin_; }
  std::sregex_iterator end() const { return end_; }
};

// Regexes in C++ don't support named capture. In Current, we have a workaround. Refresher:
// 1) `(blah)` is an unnamed capture group (referencable to by its index in the regex though).
// 2) `(?:blah)` is a non-capturing group (should match, but can't be referred to).
// 3) `(?<whoa>blah)` is a named capture -- the one C++ does not support -- allowing referring to "blah" as `whoa`.
// 4) Obviously, `\(` is an opening parenthesis, not the beginning of the group.
class NamedRegexCapturer {
 private:
  struct Data {
    std::vector<std::string> group_names;
    std::unordered_map<std::string, size_t> group_indexes;
    std::string transformed_re_body;  // This should be long-lived too.
    std::regex transformed_re;

    template <typename S>
    Data(S&& re_body) {
      // This regex requires explanation.
      // 1) Must begin with a non-escaped "(".
      // 2) Must not begin with "(?:".
      // 3) May, but does not have to be "(?<...>").
      const std::regex re_capture_groups("\\((?!\\?:)(\\?<(\\w+)>)?");
      for (const auto& capture_group : current::strings::IterateByRegexMatches(re_capture_groups, re_body)) {
        if (!capture_group.position() || re_body[capture_group.position() - 1u] != '\\') {
          const std::string name = capture_group[2].str();
          if (!name.empty()) {
            group_indexes[name] = group_names.size() + 1u;
          }
          group_names.push_back(name);
        }
      }
      transformed_re_body = std::regex_replace(re_body, re_capture_groups, "(");
      transformed_re = std::regex(std::move(transformed_re_body));
    }
  };
  std::shared_ptr<Data> data_;

 public:
  template <typename S>
  NamedRegexCapturer(S&& re_body) : data_(std::make_shared<Data>(std::forward<S>(re_body))) {}

  NamedRegexCapturer() = delete;
  NamedRegexCapturer(const NamedRegexCapturer&) = delete;
  NamedRegexCapturer& operator=(const NamedRegexCapturer&) = delete;
  NamedRegexCapturer(NamedRegexCapturer&&) = delete;
  NamedRegexCapturer& operator=(NamedRegexCapturer&&) = delete;

  // A simple test.
  bool Test(const std::string& s) const {
    std::smatch unused_match;
    return std::regex_match(s, unused_match, data_->transformed_re);
  }

  // The real match.
  struct MatchResult {
    std::shared_ptr<NamedRegexCapturer::Data> data;
    const std::smatch match;

    MatchResult() = delete;
    MatchResult(std::shared_ptr<NamedRegexCapturer::Data> data, std::smatch match)
        : data(std::move(data)), match(std::move(match)) {}

    size_t length() const { return match.length(); }
    size_t position() const { return match.position(); }
    template <typename S>
    bool Has(S&& s) const {
      const auto cit = data->group_indexes.find(s);
      if (cit != data->group_indexes.end()) {
        const size_t i = cit->second;
        if (i < match.size()) {
          return match[i].length() > 0u;
        }
      }
      return false;
    }
    template <typename S>
    std::string operator[](S&& s) const {
      const auto cit = data->group_indexes.find(s);
      if (cit != data->group_indexes.end()) {
        const size_t i = cit->second;
        if (i < match.size()) {
          return match[i].str();
        }
      }
      return "";
    }
  };

  MatchResult Match(const std::string& s) const {
    std::smatch match;
    std::regex_match(s, match, data_->transformed_re);
    return MatchResult(data_, match);
  }

  // Iterator.
  struct Iterable {
    const std::shared_ptr<NamedRegexCapturer::Data> data_;
    const std::string owned_string_;
    const std::sregex_iterator begin_;
    const std::sregex_iterator end_ = std::sregex_iterator();

    // NOTE(dkorolev): Just `const std::string& s` doesn't nail it here, as the string object itself should live
    // while the regex is being applied to it. The user is encouraged to `std::move()` a string into this code,
    // and passing in a plain C string would do the job just fine as well. -- D.K.
    Iterable(std::shared_ptr<NamedRegexCapturer::Data> data, std::string s)
      : data_(std::move(data)),
        owned_string_(std::move(s)),
        begin_(owned_string_.begin(), owned_string_.end(), data_->transformed_re) {}

    struct Iterator {
      const NamedRegexCapturer::Data& data_;
      std::sregex_iterator iterator_;
      Iterator(const NamedRegexCapturer::Data& data, std::sregex_iterator iterator)
          : data_(data), iterator_(iterator) {}
      void operator++() { ++iterator_; }
      bool operator==(const Iterator& rhs) const { return iterator_ == rhs.iterator_; }  // No need to compare `data_.`
      bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }
      struct Accessor {
        const NamedRegexCapturer::Data& data;
        std::smatch match;
        Accessor(const NamedRegexCapturer::Data& data, std::smatch match) : data(data), match(match) {}
        size_t length() const { return match.length(); }
        size_t position() const { return match.position(); }
        std::string str() const { return match.str(); }
        const std::smatch& smatch() const { return match; }
        template <typename S>
        bool Has(S&& s) const {
          const auto cit = data.group_indexes.find(s);
          if (cit != data.group_indexes.end()) {
            const size_t i = cit->second;
            if (i < match.size()) {
              return match[i].length() > 0u;
            }
          }
          return false;
        }
        template <typename S>
        std::string operator[](S&& s) const {
          const auto cit = data.group_indexes.find(s);
          if (cit != data.group_indexes.end()) {
            const size_t i = cit->second;
            if (i < match.size()) {
              return match[i].str();
            }
          }
          return "";
        }
      };
      Accessor operator*() const {
        return Accessor(data_, *iterator_);
      }
    };
    Iterator begin() const { return Iterator(*data_, begin_); }
    Iterator end() const { return Iterator(*data_, end_); }
  };

  // NOTE(dkorolev): Just `const std::string& s` doesn't nail it here, as the string itself should live
  // while the regex is being applied to it. Hence the workaround.
  template <typename S>
  Iterable Iterate(S&& s) const { return Iterable(data_, std::forward<S>(s)); }

  // Various getters.
  size_t TotalCaptures() const { return data_->group_names.size(); }
  size_t NamedCaptures() const { return data_->group_indexes.size(); }
  const std::string& GetTransformedRegexBody() const { return data_->transformed_re_body; }
  const std::regex& GetTransformedRegex() const { return data_->transformed_re; }
  const std::vector<std::string>& GetTransformedRegexCaptureGroupNames() const { return data_->group_names; }
  const std::unordered_map<std::string,size_t>& GetTransformedRegexCaptureGroupIndexes() const {
    return data_->group_indexes;
  }
};

}  // namespace strings
}  // namespace current

#endif  // BRICKS_STRINGS_ESCAPE_H
