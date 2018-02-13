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

// clang-format off

#define CURRENT_HTML_TAG_COUNT_3(tag, param1, param2)                                                    \
  struct tag final {                                                                                     \
    struct Params {                                                                                      \
      std::string value_##param1;                                                                        \
      std::string value_##param2;                                                                        \
      template <typename S> Params& param1(S&& s) { value_##param1 = std::forward<S>(s); return *this; } \
      template <typename S> Params& param2(S&& s) { value_##param2 = std::forward<S>(s); return *this; } \
    };                                                                                                   \
    std::ostream& os;                                                                                    \
    tag(::current::html::HTMLGeneratorScope& scope, const char*, int, const Params& params = Params())   \
        : os(scope.OutputStream()) {                                                                     \
      os << "<" #tag;                                                                                    \
      if (!params.value_##param1.empty()) { os << " " #param1 "='" << params.value_##param1 << "'"; }    \
      if (!params.value_##param2.empty()) { os << " " #param2 "='" << params.value_##param2 << "'"; }    \
      os << '>';                                                                                         \
    }                                                                                                    \
    ~tag() { os << "</" #tag ">"; }                                                                      \
    tag& SUERW() { return *this; }                                                                       \
  }

#define CURRENT_HTML_TAG_COUNT_2(tag, param) CURRENT_HTML_TAG_COUNT_3(tag, param, crnt_notag_1)
#define CURRENT_HTML_TAG_COUNT_1(tag) CURRENT_HTML_TAG_COUNT_3(tag, crnt_notag_1, crnt_notag_2)

// clang-format on

#define CURRENT_HTML_TAG_NARGS_IMPL(_1, _2, _3, n, ...) n
#define CURRENT_HTML_TAG_NARGS_IMPL_CALLER(args) CURRENT_HTML_TAG_NARGS_IMPL args

#define CURRENT_HTML_TAG_NARGS(...) CURRENT_HTML_TAG_NARGS_IMPL_CALLER((__VA_ARGS__, 3, 2, 1, 0))

#define CURRENT_HTML_TAG_CASE_3(n) CURRENT_HTML_TAG_COUNT_##n
#define CURRENT_HTML_TAG_CASE_2(n) CURRENT_HTML_TAG_CASE_3(n)
#define CURRENT_HTML_TAG_CASE_1(n) CURRENT_HTML_TAG_CASE_2(n)
#define CURRENT_HTML_TAG_SWITCH_N(n) CURRENT_HTML_TAG_CASE_1(n)

#define CURRENT_HTML_TAG_SWITCH(x, y) x y
#define CURRENT_HTML_TAG(...) \
  CURRENT_HTML_TAG_SWITCH(CURRENT_HTML_TAG_SWITCH_N(CURRENT_HTML_TAG_NARGS(__VA_ARGS__)), (__VA_ARGS__))

#define CURRENT_HTML_START_ONLY_TAG(tag)                                                           \
  struct tag final {                                                                               \
    std::ostream& os;                                                                              \
    tag(::current::html::HTMLGeneratorScope& scope, const char*, int) : os(scope.OutputStream()) { \
      os << "<" #tag ">";                                                                          \
    }                                                                                              \
    tag& SUERW() { return *this; }                                                                 \
  }

CURRENT_HTML_TAG(html);

CURRENT_HTML_TAG(head);
CURRENT_HTML_TAG(title);
CURRENT_HTML_TAG(style);

CURRENT_HTML_TAG(body);

CURRENT_HTML_TAG(p);
CURRENT_HTML_TAG(b);
CURRENT_HTML_TAG(i);
CURRENT_HTML_TAG(pre);
CURRENT_HTML_TAG(font, color, size);

CURRENT_HTML_START_ONLY_TAG(br);

// CURRENT_HTML_TAG(form, method, action);
// CURRENT_HTML_START_ONLY_TAG(input, submit, value);

CURRENT_HTML_TAG(h1);
CURRENT_HTML_TAG(h2);
CURRENT_HTML_TAG(h3);

CURRENT_HTML_TAG(a, href);

CURRENT_HTML_TAG(table, border, cellpadding);
CURRENT_HTML_TAG(tr);
CURRENT_HTML_TAG(td, align, colspan);

}  // namespace ::htmltag

#endif  // BLOCKS_HTML_TAGS_H
