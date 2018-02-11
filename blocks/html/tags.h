/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2018 Dmitry "Dima" Korolev, <dmitry.korolev@gmail.com>.

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

#ifndef BLOCKS_HTML_TAGS_H
#define BLOCKS_HTML_TAGS_H

#include "html.h"

// The `htmltag` namespace is intentionally in the global scope, not within `::current`, so that it can be amended to.
namespace htmltag {

#define CURRENT_HTML_TAG_WITH_NO_PARAMETERS(tag)                                                   \
  struct tag final {                                                                               \
    std::ostream& os;                                                                              \
    tag(::current::html::HTMLGeneratorScope& scope, const char*, int) : os(scope.OutputStream()) { \
      os << "<" #tag ">";                                                                          \
    }                                                                                              \
    ~tag() { os << "</" #tag ">"; }                                                                \
    tag& SUERW() { return *this; }                                                                 \
  }

#define CURRENT_HTML_TAG_WITH_ONE_PARAMETER(tag, param)                                                \
  struct tag final {                                                                                   \
    struct Params {                                                                                    \
      std::string value_##param;                                                                       \
      template <typename S>                                                                            \
      Params& param(S&& s) {                                                                           \
        value_##param = std::forward<S>(s);                                                            \
        return *this;                                                                                  \
      }                                                                                                \
    };                                                                                                 \
    std::ostream& os;                                                                                  \
    tag(::current::html::HTMLGeneratorScope& scope, const char*, int, const Params& params = Params()) \
        : os(scope.OutputStream()) {                                                                   \
      os << "<" #tag;                                                                                  \
      if (!params.value_##param.empty()) {                                                             \
        os << " " #param "='" << params.value_##param << "'";                                          \
      }                                                                                                \
      os << '>';                                                                                       \
    }                                                                                                  \
    ~tag() { os << "</" #tag ">"; }                                                                    \
    tag& SUERW() { return *this; }                                                                     \
  }

#define CURRENT_HTML_TAG_WITH_TWO_PARAMETERS(tag, param1, param2)                                      \
  struct tag final {                                                                                   \
    struct Params {                                                                                    \
      std::string value_##param1;                                                                      \
      template <typename S>                                                                            \
      Params& param1(S&& s) {                                                                          \
        value_##param1 = std::forward<S>(s);                                                           \
        return *this;                                                                                  \
      }                                                                                                \
      std::string value_##param2;                                                                      \
      template <typename S>                                                                            \
      Params& param2(S&& s) {                                                                          \
        value_##param2 = std::forward<S>(s);                                                           \
        return *this;                                                                                  \
      }                                                                                                \
    };                                                                                                 \
    std::ostream& os;                                                                                  \
    tag(::current::html::HTMLGeneratorScope& scope, const char*, int, const Params& params = Params()) \
        : os(scope.OutputStream()) {                                                                   \
      os << "<" #tag;                                                                                  \
      if (!params.value_##param1.empty()) {                                                            \
        os << " " #param1 "='" << params.value_##param1 << "'";                                        \
      }                                                                                                \
      if (!params.value_##param2.empty()) {                                                            \
        os << " " #param2 "='" << params.value_##param2 << "'";                                        \
      }                                                                                                \
      os << '>';                                                                                       \
    }                                                                                                  \
    ~tag() { os << "</" #tag ">"; }                                                                    \
    tag& SUERW() { return *this; }                                                                     \
  }

#define CURRENT_HTML_START_ONLY_TAG(tag)                                                           \
  struct tag final {                                                                               \
    std::ostream& os;                                                                              \
    tag(::current::html::HTMLGeneratorScope& scope, const char*, int) : os(scope.OutputStream()) { \
      os << "<" #tag ">";                                                                          \
    }                                                                                              \
    tag& SUERW() { return *this; }                                                                 \
  }

CURRENT_HTML_TAG_WITH_NO_PARAMETERS(html);

CURRENT_HTML_TAG_WITH_NO_PARAMETERS(head);
CURRENT_HTML_TAG_WITH_NO_PARAMETERS(title);
CURRENT_HTML_TAG_WITH_NO_PARAMETERS(style);

CURRENT_HTML_TAG_WITH_NO_PARAMETERS(body);

CURRENT_HTML_TAG_WITH_NO_PARAMETERS(p);
CURRENT_HTML_TAG_WITH_NO_PARAMETERS(b);
CURRENT_HTML_TAG_WITH_NO_PARAMETERS(i);
CURRENT_HTML_TAG_WITH_NO_PARAMETERS(pre);
CURRENT_HTML_TAG_WITH_TWO_PARAMETERS(font, color, size);

CURRENT_HTML_START_ONLY_TAG(br);

CURRENT_HTML_TAG_WITH_NO_PARAMETERS(h1);
CURRENT_HTML_TAG_WITH_NO_PARAMETERS(h2);
CURRENT_HTML_TAG_WITH_NO_PARAMETERS(h3);

CURRENT_HTML_TAG_WITH_ONE_PARAMETER(a, href);

CURRENT_HTML_TAG_WITH_TWO_PARAMETERS(table, border, cellpadding);
CURRENT_HTML_TAG_WITH_NO_PARAMETERS(tr);
CURRENT_HTML_TAG_WITH_TWO_PARAMETERS(td, align, colspan);

}  // namespace ::htmltag

#endif  // BLOCKS_HTML_TAGS_H
