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
  const std::sregex_iterator begin_;
  const std::sregex_iterator end_ = std::sregex_iterator();

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
    const std::vector<std::string> group_names;
    const std::unordered_map<std::string, size_t> group_indexes;
    const std::regex transformed_re;
    static Data Construct(const std::string& re_body) {
      std::vector<std::string> names;
      std::unordered_map<std::string, size_t> indexes;
      // This regex requires explanation.
      // 1) Must begin with a non-escaped "(".
      // 2) Must not begin with "(?:".
      // 3) May, but does not have to be "(?<...>").
      const std::regex re_capture_groups("\\((?!\\?:)(\\?<(\\w+)>)?");
      for (const auto& capture_group : current::strings::IterateByRegexMatches(re_capture_groups, re_body)) {
        if (!capture_group.position() || re_body[capture_group.position() - 1u] != '\\') {
          const std::string name = capture_group[2].str();
          if (!name.empty()) {
            indexes[name] = names.size() + 1u;
          }
          names.push_back(name);
        }
      }
      const std::string cleaned_re_body = std::regex_replace(re_body, re_capture_groups, "(");
      return Data{names, indexes, std::regex(cleaned_re_body)};
    }
  };
  const Data data_;

 public:
  NamedRegexCapturer() = delete;
  NamedRegexCapturer(const std::string& re_body) : data_(Data::Construct(re_body)) {}

  // A simple test.
  bool Test(const std::string& s) const {
    std::smatch unused_match;
    return std::regex_match(s, unused_match, data_.transformed_re);
  }

  // The real match.
  struct MatchResult {
    const NamedRegexCapturer& self;
    const std::smatch match;
    MatchResult(NamedRegexCapturer* self_ptr, std::smatch match) : self(*self_ptr), match(std::move(match)) {}
    bool Has(const std::string& s) const {
      const auto cit = self.data_.group_indexes.find(s);
      if (cit != self.data_.group_indexes.end()) {
        const size_t i = cit->second;
        if (i < match.size()) {
          return match[i].length() > 0u;
        }
      }
      return false;
    }
    std::string operator[](const std::string& s) const {
      const auto cit = self.data_.group_indexes.find(s);
      if (cit != self.data_.group_indexes.end()) {
        const size_t i = cit->second;
        if (i < match.size()) {
          return match[i].str();
        }
      }
      return "";
    }
  };

  MatchResult Match(const std::string& s) {
    std::smatch match;
    std::regex_match(s, match, data_.transformed_re);
    return MatchResult(this, match);
  }

  // Iterator.
  struct Iterable {
    const NamedRegexCapturer& self_;
    const std::string owned_string_;
    const std::sregex_iterator begin_;
    const std::sregex_iterator end_ = std::sregex_iterator();

    // NOTE(dkorolev): Just `const std::string& s` doesn't nail it here, as the string object itself should live
    // while the regex is being applied to it. The user is encouraged to `std::move()` a string into this code,
    // and passing in a plain C string would do the job just fine as well. -- D.K.
    Iterable(const NamedRegexCapturer* self_ptr, const std::regex& re, std::string s)
        : self_(*self_ptr), owned_string_(std::move(s)), begin_(owned_string_.begin(), owned_string_.end(), re) {}

    struct Iterator {
      const NamedRegexCapturer& self_;
      std::sregex_iterator iterator_;
      Iterator(const NamedRegexCapturer& self, std::sregex_iterator iterator) : self_(self), iterator_(iterator) {}
      void operator++() { ++iterator_; }
      bool operator==(const Iterator& rhs) const { return iterator_ == rhs.iterator_; }  // No need to compare `self.`
      bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }
      struct Accessor {
        const NamedRegexCapturer& self_;
        std::sregex_iterator iterator_;
        Accessor(const NamedRegexCapturer& self, std::sregex_iterator iterator) : self_(self), iterator_(iterator) {}
        bool Has(const std::string& s) const {
          const auto cit = self_.data_.group_indexes.find(s);
          if (cit != self_.data_.group_indexes.end()) {
            const size_t i = cit->second;
            if (i < (*iterator_).size()) {
              return (*iterator_)[i].length() > 0u;
            }
          }
          return false;
        }
        std::string operator[](const std::string& s) const {
          const auto cit = self_.data_.group_indexes.find(s);
          if (cit != self_.data_.group_indexes.end()) {
            const size_t i = cit->second;
            if (i < (*iterator_).size()) {
              return (*iterator_)[i].str();
            }
          }
          return "";
        }
        std::string str() const { return (*iterator_).str(); }
      };
      Accessor operator*() const {
        return Accessor(self_, iterator_);
      }
    };
    Iterator begin() const { return Iterator(self_, begin_); }
    Iterator end() const { return Iterator(self_, end_); }
  };

  // NOTE(dkorolev): Just `const std::string& s` doesn't nail it here, as the string itself should live
  // while the regex is being applied to it. Hence the workaround.
  template <typename S>
  Iterable Iterate(S&& s) const { return Iterable(this, data_.transformed_re, std::forward<S>(s)); }

  // Various getters.
  size_t TotalCaptures() const { return data_.group_names.size(); }
  size_t NamedCaptures() const { return data_.group_indexes.size(); }
};

}  // namespace strings
}  // namespace current

#endif  // BRICKS_STRINGS_ESCAPE_H
