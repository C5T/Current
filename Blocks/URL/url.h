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

#include "../../Bricks/file/file.h"

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

// For `URL::FillObject<T, MODE = Forgiving>(...)`.
enum class FillObjectMode : bool { Forgiving = false, Strict = true };

namespace impl {

namespace {
const char* const kDefaultScheme = "http";
const char* const kDefaultHost = "localhost";
}

struct URLWithoutParametersParser {
  std::string host = "";
  mutable std::string path = "/";
  std::string scheme = kDefaultScheme;
  uint16_t port = 0;
  std::string username = "";
  std::string password = "";

 protected:
  URLWithoutParametersParser() = default;

  void ParseURLWithoutParameters(const std::string& url) {
    if (url.empty()) {
      CURRENT_THROW(EmptyURLException());
    }

    // Can parse a full URL: `scheme://host.name:80/path`, `scheme://host.name/path`.
    // Can parse a protocol-relative URL: `//host.name:80/path`, `//host.name/path`.
    // Can parse a username-password pair: `http://user1:pass234@host.name:123/`.
    // Can parse a username only: `http://user1@host.name:123/`.
    // Can parse a port-only URL: `:12345/path`.
    // Can parse a relative URL: `/path/anything`.
    // Query string and fragment identifier are parsed separately in `URLParametersExtractor`.

    scheme = "";
    size_t parsed_offset = 0;

    // This piece parses scheme.
    const size_t double_slash = url.find("//");
    if (double_slash != std::string::npos && (double_slash == 0 || url[double_slash - 1] == ':')) {
      scheme = (double_slash == 0 ? "" : url.substr(0, double_slash - 1));
      parsed_offset = double_slash + 2;
    }

    const size_t host_end = url.find('/', parsed_offset);

    // This piece parses optional username-password pair with optional password: `user:pass@` or `user@`.
    const size_t at_sign = url.find('@', parsed_offset);
    if (at_sign != std::string::npos && at_sign < host_end) {
      const size_t user_pass_colon = url.find(':', parsed_offset);
      if (user_pass_colon != std::string::npos && user_pass_colon < at_sign) {
        username = url.substr(parsed_offset, user_pass_colon - parsed_offset);
        password = url.substr(user_pass_colon + 1, at_sign - (user_pass_colon + 1));
      } else {
        username = url.substr(parsed_offset, at_sign - parsed_offset);
      }
      parsed_offset = at_sign + 1;
    }

    // This piece parses host and optional port.
    const size_t port_colon = url.find(':', parsed_offset);
    if (port_colon != std::string::npos && port_colon < host_end) {
      host = url.substr(parsed_offset, port_colon - parsed_offset);
      port = static_cast<uint16_t>(atoi(url.c_str() + port_colon + 1));
    } else {
      host = url.substr(parsed_offset, host_end - parsed_offset);
    }

    // This piece parses path.
    if (host_end != std::string::npos) {
      path = url.substr(host_end);
    } else {
      path = "";
    }
    if (path.empty()) {
      path = "/";
    }
  }

  std::string ComposeURL() const {
    if (!host.empty()) {
      const std::string scheme_for_compose =
          (!scheme.empty() ? scheme : (port > 0 ? DefaultSchemeForPort(port) : kDefaultScheme));
      const uint16_t port_for_compose = (port > 0 && port != DefaultPortForScheme(scheme_for_compose) ? port : 0);

      std::ostringstream os;
      os << (!scheme_for_compose.empty() ? scheme_for_compose : kDefaultScheme) << "://";
      if (!username.empty()) {
        os << username;
        if (!password.empty()) {
          os << ':' << password;
        }
        os << '@';
      }
      os << host;
      if (port_for_compose > 0) {
        os << ':' << port_for_compose;
      }
      os << path;
      return os.str();
    } else {
      // If no host is specified, it's just the path: No need to put scheme and port.
      return path;
    }
  }

 public:
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
 protected:
  URLParametersExtractor() = default;

  void ExtractURLParametersFromURL(std::string url, size_t& parameters_start_index) {
    parameters_start_index = url.length();
    const size_t pound_sign_index = url.find('#');
    if (pound_sign_index != std::string::npos) {
      fragment = url.substr(pound_sign_index + 1);
      parameters_start_index = pound_sign_index;
    }
    const size_t question_mark_index = url.find('?');
    if (question_mark_index != std::string::npos && question_mark_index < pound_sign_index) {
      auto& v = parameters_vector;
      current::strings::Split(url.substr(question_mark_index + 1, parameters_start_index - (question_mark_index + 1)),
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
      parameters_start_index = question_mark_index;
    }
  }

 public:
  static bool IsHexDigit(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

  static bool IsURIComponentSpecialCharacter(char c) {
    // https://github.com/lyokato/cpp-urilite/blob/723083f98ddf42f2b610b62444c598236b5ce3e8/include/urilite.h#L52-L65
    // RFC2396 unreserved?
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~' || c == '!' || c == '\'' || c == '(' || c == ')');
  }

  static std::string DecodeURIComponent(const std::string& encoded) {
    std::ostringstream decoded;
    for (size_t i = 0; i < encoded.length(); ++i) {
      const char c = encoded[i];
      if (c == '+') {
        decoded << ' ';
      } else if (c == '%' && i + 3 <= encoded.length() && IsHexDigit(encoded[i + 1]) && IsHexDigit(encoded[i + 2])) {
        decoded << static_cast<char>(std::stoi(encoded.substr(i + 1, 2).c_str(), nullptr, 16));
        i += 2;
      } else {
        decoded << c;
      }
    }
    return decoded.str();
  }

  static std::string EncodeURIComponent(const std::string& decoded) {
    std::ostringstream encoded;
    for (const char c : decoded) {
      if (IsURIComponentSpecialCharacter(c)) {
        encoded << c;
      } else if (c == ' ') {
        encoded << "%20";
      } else {
        encoded << current::strings::Printf("%%%02X", static_cast<int>(c));
      }
    }
    return encoded.str();
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
    template <typename T, FillObjectMode MODE = FillObjectMode::Forgiving>
    std::enable_if_t<IS_CURRENT_STRUCT(T), const T&> FillObject(T& object) const {
      FillObjectImpl<T, MODE>::DoIt(parameters_, object);
      return object;
    }

    template <typename T, FillObjectMode MODE = FillObjectMode::Forgiving>
    T FillObject() const {
      T object;
      FillObject<T, MODE>(object);
      return object;
    }

   private:
    template <typename T, FillObjectMode MODE>
    struct FillObjectImpl {
      static void DoIt(const std::map<std::string, std::string>& parameters, T& object) {
        using decayed_t = current::decay<T>;
        using super_t = current::reflection::SuperType<decayed_t>;
        FillObjectImpl<super_t, MODE>::DoIt(parameters, static_cast<super_t&>(object));
        QueryParametersObjectFiller<T, MODE> parser{parameters};
        current::reflection::VisitAllFields<T, current::reflection::FieldNameAndMutableValue>::WithObject(object, parser);
      }
    };

    template <FillObjectMode MODE>
    struct FillObjectImpl<CurrentStruct, MODE> {
      static void DoIt(const std::map<std::string, std::string>&, CurrentStruct&) {}
    };

    template <typename TOP_LEVEL_T, FillObjectMode MODE>
    struct QueryParametersObjectFiller {
      const std::map<std::string, std::string>& q;

      // The `std::string` is a special case, as no quotes are expected.
      void operator()(const std::string& key, std::string& value) const {
        const auto cit = q.find(key);
        if (cit != q.end()) {
          value = cit->second;
        } else if (MODE == FillObjectMode::Strict) {
          CURRENT_THROW(URLParseSpecificObjectAsURLParameterException<TOP_LEVEL_T>(key, "missing value"));
        }
      }

      // The `bool` is a special case as well, as a) just `?b` should set `b` to true,
      // and b) Current supports 0/1, false/true, False/True, and FALSE/TRUE.
      void operator()(const std::string& key, bool& value) const {
        const auto cit = q.find(key);
        if (cit != q.end()) {
          if (cit->second.empty()) {
            value = true;  // Just `?b` sets `b` to true.
          } else if (MODE == FillObjectMode::Forgiving) {
            current::FromString(cit->second, value);
          } else {
            try {
              ParseJSON(cit->second, value);
            } catch (const current::TypeSystemParseJSONException& exception) {
              CURRENT_THROW(
                  URLParseSpecificObjectAsURLParameterException<TOP_LEVEL_T>(key, exception.OriginalDescription()));
            }
          }
        }
      }

      // `Optional<>`-s are special cases, as they can be missing.
      template <typename T>
      void operator()(const std::string& key, Optional<T>& value) const {
        const auto cit = q.find(key);
        if (cit != q.end()) {
          value = T();
          operator()(key, Value(value));
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
            CURRENT_THROW(
                URLParseSpecificObjectAsURLParameterException<TOP_LEVEL_T>(key, exception.OriginalDescription()));
          }
        } else if (MODE == FillObjectMode::Strict) {
          CURRENT_THROW(URLParseSpecificObjectAsURLParameterException<TOP_LEVEL_T>(key, "missing value"));
        }
      }
    };
  };

  const std::map<std::string, std::string>& AllQueryParameters() const { return query.AsImmutableMap(); }

  std::vector<std::pair<std::string, std::string>> parameters_vector;
  QueryParameters query;
  std::string fragment;
};

struct URL : URLParametersExtractor, URLWithoutParametersParser {
  URL() = default;

  URL(const std::string& url) {
    size_t parameters_start_index = 0;
    URLParametersExtractor::ExtractURLParametersFromURL(url, parameters_start_index);
    URLWithoutParametersParser::ParseURLWithoutParameters(url.substr(0, parameters_start_index));
  }

  std::string ComposeURLWithoutParameters() const { return URLWithoutParametersParser::ComposeURL(); }

  std::string ComposeURL() const {
    return URLWithoutParametersParser::ComposeURL() + URLParametersExtractor::ComposeParameters();
  }

  static URL MakeURLWithDefaults(const URL& source_url_parsed) {
    URL url = URL(source_url_parsed);

    if (url.scheme.empty()) {
      if (url.port == 0) {
        url.scheme = kDefaultScheme;
      } else {
        url.scheme = DefaultSchemeForPort(url.port);
        if (url.scheme.empty()) {
          url.scheme = kDefaultScheme;
        }
      }
    }

    if (url.host.empty()) {
      url.host = kDefaultHost;
    }

    if (url.port == 0) {
      url.port = DefaultPortForScheme(url.scheme);
    }

    return url;
  }

  static URL MakeRedirectedURL(const URL& source_url_parsed, const URL& target_url_parsed) {
    URL url = URL(source_url_parsed);

    if (!target_url_parsed.scheme.empty()) {
      url.scheme = target_url_parsed.scheme;
    }

    url.username = target_url_parsed.username;
    url.password = target_url_parsed.password;

    if (!target_url_parsed.host.empty()) {
      url.host = target_url_parsed.host;
      url.port = target_url_parsed.port;
    } else if (target_url_parsed.port > 0) {
      url.port = target_url_parsed.port;
    }

    url.path = target_url_parsed.path;

    url.parameters_vector = target_url_parsed.parameters_vector;
    url.query = target_url_parsed.query;
    url.fragment = target_url_parsed.fragment;

    return MakeURLWithDefaults(url);
  }

  static URL MakeRedirectedURL(const URL& source_url_parsed, const std::string& url) {
    return URL::MakeRedirectedURL(source_url_parsed, URL(url));
  }

  // Parses query strings without the question mark and `application/x-www-form-urlencoded` HTTP bodies.
  static const std::map<std::string, std::string> ParseQueryString(const std::string& body) {
    return URL("/?" + body).AllQueryParameters();
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

  std::string ComposeURLPathFromArgs() const {
    std::string result;
    for (const auto& component : (*this)) {
      result += "/" + component;
    }
    return (result.empty() ? "/" : result);
  }

  std::string ComposeURLPath() const {
    const std::string path_from_args = ComposeURLPathFromArgs();
    if (base_path.empty() || base_path == "/") {
      return path_from_args;
    } else {
      return (path_from_args == "/" ? base_path : base_path + path_from_args);
    }
  }

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
