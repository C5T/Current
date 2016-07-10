/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_NET_HTTP_HEADERS_HEADERS_H
#define BRICKS_NET_HTTP_HEADERS_HEADERS_H

#include "../../../port.h"

#include <algorithm>
#include <cassert>
#include <map>
#include <string>
#include <vector>

#include "../../exceptions.h"

#include "../../../time/chrono.h"
#include "../../../strings/split.h"
#include "../../../strings/util.h"

namespace current {
namespace net {
namespace http {

struct HeaderNotFoundException : Exception {
  explicit HeaderNotFoundException(const std::string& h) : Exception("Header not found: `" + h + "`.") {}
};

struct CookieIsNotYourRegularHeader : Exception {
  using Exception::Exception;
};

struct InvalidHTTPDateException : Exception {
  explicit InvalidHTTPDateException(const std::string& d) : Exception("Unparsable date: `" + d + "`.") {}
};

// Default `padding` to `Upper` (one microsecond left to the beginning of the next second)
// to err on the right side when it comes to `If-Unmodified-Since` logic. -- D.K.
inline std::chrono::microseconds ParseHTTPDate(
    const std::string& datetime,
    current::time::SecondsToMicrosecondsPadding padding = current::time::SecondsToMicrosecondsPadding::Upper) {
  // Try IMF-fixdate/RFC1123 format.
  auto t = current::IMFFixDateTimeStringToTimestamp(datetime, padding);
  if (t.count()) {
    return t;
  }
  // Try RFC850 format.
  t = current::RFC850DateTimeStringToTimestamp(datetime, padding);
  if (t.count()) {
    return t;
  } else {
    // TODO: Support ANSI C format as well.
    CURRENT_THROW(InvalidHTTPDateException(datetime));
  }
}

// A generic implementation for HTTP headers.
// * Shared for client- and server-side use (set by the user or set by an internal HTTP response parser).
// * Preserves the order in which headers have been added.
// * Supports lookup by key as well, in case-insensitive way.
// * Concatenates everything but cookies via ", ", stores cookies as a map.
// * Preserves case, but allows case-insensitive iteration and lookup. The latter also equalized '-' and '_'.
struct Header final {
  const std::string header;  // Immutable as map key, even for a mutable `Header`.
  std::string value;

  Header() = delete;  // And `header`, the name of the header as string, has to be initalized explicitly.
  explicit Header(const std::string& header) : header(header) { ThrowIfHeaderIsCookie(header); }

  Header(const std::string& header, const std::string& value) : header(header), value(value) {
    ThrowIfHeaderIsCookie(header);
  }

  Header(const Header&) = default;
  Header(Header&&) = default;
  Header& operator=(const Header&) = default;
  Header& operator=(Header&&) = default;

  Header& operator=(const std::string& rhs) {
    value = rhs;
    return *this;
  }

  // Case-insensitive and {'-'/'_'}-insensitive comparison.
  struct KeyComparator final {
    char Canonical(char c) const {
      if (c == '-') {
        return '_';
      } else if (c >= 'A' && c <= 'Z') {
        return c + 'a' - 'A';
      } else {
        return c;
      }
    }
    bool operator()(const std::string& lhs, const std::string& rhs) const {
      const size_t lhs_length = lhs.length();
      const size_t rhs_length = rhs.length();
      const size_t min_length = std::min(lhs_length, rhs_length);
      auto lhs_it = lhs.begin();
      auto rhs_it = rhs.begin();
      for (size_t i = 0; i < min_length; ++i, ++lhs_it, ++rhs_it) {
        const char a = Canonical(*lhs_it);
        const char b = Canonical(*rhs_it);
        if (a < b) {
          return true;
        } else if (b < a) {
          return false;
        }
      }
      return lhs_length < rhs_length;
    }
  };

  // Make sure `Cookies` are never accessed as regular headers.
  static bool IsCookieHeader(const std::string& header) {
    const char* kClientCookieHeaderName = "Cookie";
    const char* kServerCookieHeaderName = "Set-Cookie";
    const auto cmp = Header::KeyComparator();
    // Use equivalency to emulate equality. -- D.K.
    return (!cmp(header, kClientCookieHeaderName) && !cmp(kClientCookieHeaderName, header)) ||
           (!cmp(header, kServerCookieHeaderName) && !cmp(kServerCookieHeaderName, header));
  }

  static void ThrowIfHeaderIsCookie(const std::string& header) {
    if (IsCookieHeader(header)) {
      CURRENT_THROW(CookieIsNotYourRegularHeader());
    }
  }
};

struct Cookie final {
  std::string value;
  std::map<std::string, std::string> params;
  Cookie(const std::string& value = "") : value(value) {}
  Cookie& operator=(const std::string& new_value) {
    value = new_value;
    return *this;
  }
  std::string& operator[](const std::string& param_name) { return params[param_name]; }
};

struct Headers final {
  // An `std::unique_ptr<>` to preserve the address as the map grows. For case-insensitive lookup.
  using headers_map_t = std::map<std::string, std::unique_ptr<Header>, Header::KeyComparator>;
  // A list preserving the order, pointing to persisted Header-s contained in `std::unique_ptr<>` of `std::map`.
  using headers_list_t = std::vector<std::pair<std::string, Header*>>;
  // Cookies as a map of `std::string` cookie name into the value of this cookie and extra parameters for it.
  using cookies_map_t = std::map<std::string, Cookie>;

  using DEPRECATED_T_(HEADERS_MAP) = headers_map_t;
  using DEPRECATED_T_(HEADERS_LIST) = headers_list_t;
  using DEPRECATED_T_(COOKIES_MAP) = cookies_map_t;

  Headers() = default;
  Headers(Headers&&) = default;
  Headers& operator=(Headers&&) = default;

  void DeepCopyFrom(const Headers& rhs) {
    for (const auto& h : rhs.map) {
      map[h.first] = std::make_unique<Header>(*h.second);
    }
    for (const auto& h : rhs.list) {
      list.emplace_back(h.first, map[h.first].get());
    }
    cookies = rhs.cookies;
  }

  Headers(const Headers& rhs) { DeepCopyFrom(rhs); }

  Headers& operator=(const Headers& rhs) {
    DeepCopyFrom(rhs);
    return *this;
  }

  Headers(const std::string& header, const std::string& value) { Set(header, value); }
  Headers(std::initializer_list<std::pair<std::string, std::string>> initializer) {
    std::for_each(initializer.begin(),
                  initializer.end(),
                  [this](const std::pair<std::string, std::string>& h) { Set(h.first, h.second); });
  }

  // Can `Set()` / `Get()` / `Has()` headers.
  Headers& Set(const std::string& header, const std::string& value) {
    Header::ThrowIfHeaderIsCookie(header);
    // Discard empty headers.
    if (!header.empty()) {
      std::unique_ptr<Header>& placeholder = map[header];
      if (!placeholder) {
        // Create new `Header` under this key.
        placeholder = std::make_unique<Header>(header);
        list.emplace_back(header, placeholder.get());
      }
      // Discard empty values.
      // Exception: if the only value(s) for this key have been empty, the resulting value should be empty too.
      if (!value.empty()) {
        if (!placeholder->value.empty()) {
          placeholder->value += ", ";
        }
        placeholder->value += value;
      }
    }
    return *this;
  }

  bool Has(const std::string& header) const {
    Header::ThrowIfHeaderIsCookie(header);
    return map.find(header) != map.end();
  }

  const std::string& Get(const std::string& header) const {
    Header::ThrowIfHeaderIsCookie(header);
    const auto rhs = map.find(header);
    if (rhs != map.end()) {
      assert(rhs->second);  // Invariant: If `header` exists in map, its `std::unique_ptr<>` value is valid.
      return rhs->second->value;
    } else {
      CURRENT_THROW(HeaderNotFoundException(header));
    }
  }

  std::string GetOrDefault(const std::string& header, const std::string& value_if_not_found) const {
    Header::ThrowIfHeaderIsCookie(header);
    const auto rhs = map.find(header);
    if (rhs != map.end()) {
      assert(rhs->second);  // Invariant: If `header` exists in map, its `std::unique_ptr<>` value is valid.
      return rhs->second->value;
    } else {
      return value_if_not_found;
    }
  }

  // Can `operator[] const` headers. Returns the `Header`, with `.header` and `.value`.
  // Capitalization-wise, treats various spellings of the same header as one header, retains the name first
  // seen.
  const Header& operator[](const std::string& header) const {
    Header::ThrowIfHeaderIsCookie(header);
    const auto rhs = map.find(header);
    if (rhs != map.end()) {
      assert(rhs->second);  // Invariant: If `header` exists in map, its `std::unique_ptr<>` value is valid.
      return *rhs->second;
    } else {
      CURRENT_THROW(HeaderNotFoundException(header));
    }
  }

  // Can `Header& operator[]`. Creates the header if it does not exist. Its `.header` is const, `.value`
  // mutable.
  // Capitalization-wise, treats various spellings of the same header as one header, retains the name first
  // seen.
  Header& operator[](const std::string& header) {
    Header::ThrowIfHeaderIsCookie(header);
    std::unique_ptr<Header>& placeholder = map[header];
    if (!placeholder) {
      // Create new `Header` under this `header`.
      placeholder = std::make_unique<Header>(header);
      list.emplace_back(header, placeholder.get());
    }
    return *placeholder.get();
  }

  // STL-style accessors. Note that `Cookies` are not counted as regular headers.
  bool empty() const { return list.empty(); }

  size_t size() const { return list.size(); }

  // Allow only const iteration.
  struct Iterator final {
    typename headers_list_t::const_iterator iterator;

    bool operator==(const Iterator& rhs) const { return iterator == rhs.iterator; }
    bool operator!=(const Iterator& rhs) const { return iterator != rhs.iterator; }

    const Header& operator*() const { return *iterator->second; }

    const Header* operator->() const { return iterator->second; }

    void operator++() { ++iterator; }
  };

  Iterator begin() const { return Iterator{list.begin()}; }
  Iterator end() const { return Iterator{list.end()}; }

  // Legacy.
  void push_back(const std::pair<std::string, std::string>& pair) { Set(pair.first, pair.second); }
  void emplace_back(const std::string& header, const std::string& value) { Set(header, value); }

  // An interface for the HTTP library to populate headers or cookies.
  void ApplyCookieHeader(const std::string& value) {
    for (const auto& kv : strings::SplitIntoKeyValuePairs(value, '=', ';')) {
      cookies.emplace(strings::Trim(kv.first), strings::Trim(kv.second));
    }
  }
  void SetHeaderOrCookie(const std::string& header, const std::string& value) {
    if (Header::IsCookieHeader(header)) {
      ApplyCookieHeader(value);
    } else {
      Set(header, value);
    }
  }

  // An externally facing `SetCookie()`, to be used mostly while constructing the `Response`.
  Headers& SetCookie(const std::string& name, const std::string& value) {
    cookies[name] = value;
    return *this;
  }

  Headers& SetCookie(const std::string& name, const Cookie& cookie) {
    cookies[name] = cookie;
    return *this;
  }

  Headers& SetCookie(const std::string& name,
                     const std::string& value,
                     const std::map<std::string, std::string>& params) {
    auto& placeholder = cookies[name];
    placeholder.value = value;
    placeholder.params = params;
    return *this;
  }

  // Return all headers, but not cookes, as a single `std::map<std::string, std::string>`.
  std::map<std::string, std::string> AsMap() const {
    std::map<std::string, std::string> headers;
    for (const auto& h : *this) {
      headers.emplace(h.header, h.value);
    }
    return headers;
  }

  std::string CookiesAsString() const {
    if (cookies.empty()) {
      return "";
    } else {
      std::string cookies_as_string;
      for (const auto& c : cookies) {
        if (!cookies_as_string.empty()) {
          cookies_as_string += "; ";
        }
        cookies_as_string += c.first + '=' + c.second.value;
        // TODO(dkorolev): Extra parameters for this cookie.
      }
      return cookies_as_string;
    }
  }

  // `map[header]` either does not exist, or contains a valid `std::unique_ptr<Header>`.
  headers_map_t map;

  // `list[i]` points to the underlying `Header*` of the `std::unique_ptr<Header>` stored in `map`,
  // respecting the order in which the headers have been added.
  headers_list_t list;

  // Cookies are just an `std::map<std::string, Cookie>`.
  cookies_map_t cookies;
};

}  // namespace http
}  // namespace net
}  // namespace current

#endif  // BRICKS_NET_HTTP_HEADERS_HEADERS_H
