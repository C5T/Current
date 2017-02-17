/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev, <dmitry.korolev@gmail.com>.

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

#ifndef BLOCKS_URL_URL_H
#define BLOCKS_URL_URL_H

#include "../../port.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "exceptions.h"

#include "../../TypeSystem/Reflection/reflection.h"
#include "../../TypeSystem/Serialization/json.h"

#include "../../Bricks/exception.h"

#include "../../Bricks/strings/printf.h"
#include "../../Bricks/strings/split.h"

namespace current {
namespace url {

struct EmptyURLException : current::Exception {};
// struct EmptyURLHostException : Exception {};

// URL manages the mapping between the string and parsed representations of the URL. It manages:
//
// * host    (string)
// * path    (string, defaults to "/", never empty.)
// * scheme  (defaults to "http", empty only if set explicitly in constructor.)
// * port    (defaults to the default port for supported schemes, zero/unset for unknown ones.)
//
// When handling redirects, the previous URL can be provided to properly handle host/port/scheme.

namespace impl {

namespace {
const char* const kDefaultScheme = "http";
}

struct URLWithoutParametersParser {
  std::string host = "";
  mutable std::string path = "/";
  std::string scheme = kDefaultScheme;
  int port = 0;

  URLWithoutParametersParser() = default;

  // Extra parameters for previous host and port are provided in the constructor to handle redirects.
  URLWithoutParametersParser(const std::string& url,
                             const std::string& previous_scheme = kDefaultScheme,
                             const std::string& previous_host = "",
                             const int previous_port = 0) {
    if (url.empty()) {
      CURRENT_THROW(EmptyURLException());
    }
    scheme = "";
    size_t offset_past_scheme = 0;
    const size_t i = url.find("://");
    if (i != std::string::npos) {
      scheme = url.substr(0, i);
      offset_past_scheme = i + 3;
    }

    // TODO(dkorolev): Support `http://user:pass@host:80/` in the future.
    const size_t colon = url.find(':', offset_past_scheme);
    const size_t slash = url.find('/', offset_past_scheme);
    host = url.substr(offset_past_scheme, std::min(colon, slash) - offset_past_scheme);
    if (host.empty()) {
      host = previous_host;
    }

    if (colon < slash) {
      port = atoi(url.c_str() + colon + 1);
    } else {
      port = previous_port;
    }

    if (slash != std::string::npos) {
      path = url.substr(slash);
    } else {
      path = "";
    }
    if (path.empty()) {
      path = "/";
    }

    if (scheme.empty()) {
      if (!previous_scheme.empty()) {
        scheme = previous_scheme;
      } else {
        scheme = DefaultSchemeForPort(port);
      }
    }

    if (port == 0) {
      port = DefaultPortForScheme(scheme);
    }
  }

  URLWithoutParametersParser(const std::string& url, const URLWithoutParametersParser& previous)
      : URLWithoutParametersParser(url, previous.scheme, previous.host, previous.port) {}

  std::string ComposeURL() const {
    if (!host.empty()) {
      std::ostringstream os;
      if (!scheme.empty()) {
        os << scheme << "://";
      }
      os << host;
      if (port != DefaultPortForScheme(scheme)) {
        os << ':' << port;
      }
      os << path;
      return os.str();
    } else {
      // If no host is specified, it's just the path: No need to put scheme and port.
      return path;
    }
  }

  static int DefaultPortForScheme(const std::string& scheme) {
    // We don't really "support" other schemes yet -- D.K.
    if (scheme == "http") {
      return 80;
    } else if (scheme == "https") {
      return 443;
    } else {
      return 0;
    }
  }

  static std::string DefaultSchemeForPort(int port) {
    if (port == 80) {
      return "http";
    } else if (port == 443) {
      return "https";
    } else {
      return "";
    }
  }
};

struct URLParametersExtractor {
  URLParametersExtractor() = default;
  URLParametersExtractor(std::string url) {
    const size_t pound_sign_index = url.find('#');
    if (pound_sign_index != std::string::npos) {
      fragment = url.substr(pound_sign_index + 1);
      url = url.substr(0, pound_sign_index);
    }
    const size_t question_mark_index = url.find('?');
    if (question_mark_index != std::string::npos) {
      auto& v = parameters_vector;
      current::strings::Split(url.substr(question_mark_index + 1),
                              '&',
                              [&v](const std::string& chunk) {
                                const size_t i = chunk.find('=');
                                if (i != std::string::npos) {
                                  v.emplace_back(chunk.substr(0, i), chunk.substr(i + 1));
                                } else {
                                  v.emplace_back(chunk, "");
                                }
                              });
      for (auto& it : parameters_vector) {
        it.first = DecodeURIComponent(it.first);
        it.second = DecodeURIComponent(it.second);
      }
      query = QueryParameters(parameters_vector);
      url = url.substr(0, question_mark_index);
    }
    url_without_parameters = url;
  }

  static bool IsHexDigit(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

  static std::string DecodeURIComponent(const std::string& encoded) {
    std::string decoded;
    for (size_t i = 0; i < encoded.length(); ++i) {
      if (i + 3 <= encoded.length() && encoded[i] == '%' && IsHexDigit(encoded[i + 1]) && IsHexDigit(encoded[i + 2])) {
        decoded += static_cast<char>(std::stoi(encoded.substr(i + 1, 2).c_str(), nullptr, 16));
        i += 2;
      } else if (encoded[i] == '+') {
        decoded += ' ';
      } else {
        decoded += encoded[i];
      }
    }
    return decoded;
  }

  static std::string EncodeURIComponent(const std::string& decoded) {
    std::string encoded;
    for (const char c : decoded) {
      if (::isalpha(c) || ::isdigit(c)) {
        encoded += c;
      } else {
        encoded += current::strings::Printf("%%%02X", static_cast<int>(c));
      }
    }
    return encoded;
  }

  std::string ComposeParameters() const {
    std::string composed_parameters;
    for (size_t i = 0; i < parameters_vector.size(); ++i) {
      composed_parameters += "?&"[i > 0];
      composed_parameters +=
          EncodeURIComponent(parameters_vector[i].first) + '=' + EncodeURIComponent(parameters_vector[i].second);
    }
    if (!fragment.empty()) {
      composed_parameters += "#" + fragment;
    }
    return composed_parameters;
  }

  struct QueryParameters {
    QueryParameters() = default;
    QueryParameters(const std::vector<std::pair<std::string, std::string>>& parameters_vector)
        : parameters_(parameters_vector.begin(), parameters_vector.end()) {}
    inline std::string operator[](const std::string& key) const { return get(key, ""); }
    inline bool has(const std::string& key) const { return parameters_.find(key) != parameters_.end(); }
    inline const std::string& get(const std::string& key, const std::string& default_value) const {
      const auto cit = parameters_.find(key);
      if (cit != parameters_.end()) {
        return cit->second;
      } else {
        return default_value;
      }
    }
    const std::map<std::string, std::string>& AsImmutableMap() const { return parameters_; }
    std::map<std::string, std::string> parameters_;

    // NOTE: `FillObject` only populates the fields present in the URL; it doesn't erase what's not in the querystring.
    template <typename T>
    const T& FillObject(T& object) const {
      QueryParametersObjectFiller<T> parser{parameters_};
      current::reflection::VisitAllFields<T, current::reflection::FieldNameAndMutableValue>::WithObject(object, parser);
      return object;
    }

    template <typename T>
    T FillObject() const {
      T object;
      FillObject(object);
      return object;
    }

   private:
    template <typename TOP_LEVEL_T>
    struct QueryParametersObjectFiller {
      const std::map<std::string, std::string>& q;

      // The `std::string` is a special case, as no quotes are expected.
      void operator()(const std::string& key, std::string& value) const {
        const auto cit = q.find(key);
        if (cit != q.end()) {
          value = cit->second;
        }
      }

      // The `Optional<std::string>` is also a special case, as no quotes are expected.
      void operator()(const std::string& key, Optional<std::string>& value) const {
        const auto cit = q.find(key);
        if (cit != q.end()) {
          value = cit->second;
        }
      }

      // The `bool` is a special case as well, as a) just `?b` should set `b` to true,
      // and b) Current supports 0/1, false/true, False/True, and FALSE/TRUE.
      void operator()(const std::string& key, bool& value) const {
        const auto cit = q.find(key);
        if (cit != q.end()) {
          if (cit->second.empty()) {
            value = true;  // Just `?b` sets `b` to true.
          } else {
            current::FromString(cit->second, value);
          }
        }
      }

      // And `Optional<bool>` is also a special case.
      void operator()(const std::string& key, Optional<bool>& value) const {
        const auto cit = q.find(key);
        if (cit != q.end()) {
          if (cit->second.empty()) {
            value = true;  // Just `?b` sets `b` to true.
          } else {
            value = current::FromString<bool>(cit->second);
          }
        }
      }

      // For the remaining field types, use `ParseJSON`. Overkill, but ensures any `CURRENT_STRUCT` can be URL-encoded.
      template <typename T>
      void operator()(const std::string& key, T& value) const {
        const auto cit = q.find(key);
        if (cit != q.end()) {
          try {
            ParseJSON(cit->second, value);
          } catch (const current::TypeSystemParseJSONException& exception) {
            CURRENT_THROW(URLParseSpecificObjectAsURLParameterException<TOP_LEVEL_T>(key, exception.What()));
          }
        }
      }
    };
  };

  const std::map<std::string, std::string>& AllQueryParameters() const { return query.AsImmutableMap(); }

  std::vector<std::pair<std::string, std::string>> parameters_vector;
  QueryParameters query;
  std::string fragment;
  std::string url_without_parameters;
};

struct URL : URLParametersExtractor, URLWithoutParametersParser {
  URL() = default;

  // Extra parameters for previous host and port are provided in the constructor to handle redirects.
  URL(const std::string& url,
      const std::string& previous_scheme = kDefaultScheme,
      const std::string& previous_host = "",
      const int previous_port = 0)
      : URLParametersExtractor(url),
        URLWithoutParametersParser(
            URLParametersExtractor::url_without_parameters, previous_scheme, previous_host, previous_port) {}

  URL(const std::string& url, const URLWithoutParametersParser& previous)
      : URLParametersExtractor(url),
        URLWithoutParametersParser(URLParametersExtractor::url_without_parameters, previous) {}

  std::string ComposeURL() const {
    return URLWithoutParametersParser::ComposeURL() + URLParametersExtractor::ComposeParameters();
  }

  // Pretty simple, though not fully RFC-compliant way to check if URL path is valid.
  static bool IsPathValidToRegister(const std::string& path) {
    const std::set<char> valid_nonalnum_chars{
        '/', '-', '.', '_', '~', '!', '$', '&', '\'', '(', ')', '*', '+', ',', ';', '=', ':', '@'};
    return find_if(path.begin(),
                   path.end(),
                   [&valid_nonalnum_chars](char c) { return !(isalnum(c) || valid_nonalnum_chars.count(c)); }) ==
           path.end();
  }
};

}  // namespace impl

using URL = impl::URL;

// `URLPathArgs` is used to register a HTTP handler that accepts a number of parameters defined by `CountMask`.
// Examples:
//  {"/foo", CountMask::None} serves only requests to "/foo",
//  {"/foo", CountMask::One} serves only requests to "/foo/{arg}",
//  {"/foo", CountMask::None | CountMask::One} serves requests to "/foo" and "/foo/{arg}",
//  {"/foo", CountMask::Any} serves requests to "/foo", "/foo/{arg1}", "/foo/{arg1}/{arg2}...".
struct URLPathArgs {
 public:
  using CountMaskUnderlyingType = uint16_t;
  enum { MaxArgsCount = 15 };
  enum class CountMask : CountMaskUnderlyingType {
    None = (1 << 0),
    One = (1 << 1),
    Two = (1 << 2),
    Three = (1 << 3),
    Any = static_cast<CountMaskUnderlyingType>(~0)
  };

  const std::string& operator[](size_t index) const { return args_.at(args_.size() - 1 - index); }

  using iterator_t = std::vector<std::string>::const_reverse_iterator;

  iterator_t begin() const { return args_.crbegin(); }
  iterator_t end() const { return args_.crend(); }

  bool empty() const { return args_.empty(); }
  size_t size() const { return args_.size(); }

  void add(const std::string& arg) { args_.push_back(arg); }

  std::string base_path;

 private:
  std::vector<std::string> args_;
};

inline URLPathArgs::CountMask operator&(const URLPathArgs::CountMask lhs, const URLPathArgs::CountMask rhs) {
  return static_cast<URLPathArgs::CountMask>(static_cast<URLPathArgs::CountMaskUnderlyingType>(lhs) &
                                             static_cast<URLPathArgs::CountMaskUnderlyingType>(rhs));
}

inline URLPathArgs::CountMask operator|(const URLPathArgs::CountMask lhs, const URLPathArgs::CountMask rhs) {
  return static_cast<URLPathArgs::CountMask>(static_cast<URLPathArgs::CountMaskUnderlyingType>(lhs) |
                                             static_cast<URLPathArgs::CountMaskUnderlyingType>(rhs));
}

inline URLPathArgs::CountMask& operator|=(URLPathArgs::CountMask& lhs, const URLPathArgs::CountMask rhs) {
  lhs = static_cast<URLPathArgs::CountMask>(static_cast<URLPathArgs::CountMaskUnderlyingType>(lhs) |
                                            static_cast<URLPathArgs::CountMaskUnderlyingType>(rhs));
  return lhs;
}

inline URLPathArgs::CountMask operator<<(const URLPathArgs::CountMask lhs, const size_t count) {
  return static_cast<URLPathArgs::CountMask>(static_cast<URLPathArgs::CountMaskUnderlyingType>(lhs) << count);
}

inline URLPathArgs::CountMask& operator<<=(URLPathArgs::CountMask& lhs, const size_t count) {
  lhs = (lhs << count);
  return lhs;
}

}  // namespace url
}  // namespace current

using current::url::URL;
using current::url::URLPathArgs;

#endif  // BLOCKS_URL_URL_H
