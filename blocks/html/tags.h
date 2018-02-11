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

#define CURRENT_SIMPLE_HTML_TAG(tag)                                                                          \
  struct tag final {                                                                                          \
    ::current::html::HTMLGeneratingContext& ctx;                                                              \
    tag(::current::html::HTMLGeneratingContext& ctx, const char*, int) : ctx(ctx) { ctx.os << "<" #tag ">"; } \
    ~tag() { ctx.os << "</" #tag ">"; }                                                                       \
    tag& SUERW() { return *this; }                                                                            \
  };

CURRENT_SIMPLE_HTML_TAG(p);
CURRENT_SIMPLE_HTML_TAG(b);
CURRENT_SIMPLE_HTML_TAG(i);
CURRENT_SIMPLE_HTML_TAG(pre);

struct a final {
  struct Params {
    std::string value_href;
    template <typename S>
    Params& href(S&& s) {
      value_href = std::forward<S>(s);
      return *this;
    }
  };
  ::current::html::HTMLGeneratingContext& ctx;
  a(::current::html::HTMLGeneratingContext& ctx, const char*, int, const Params& params) : ctx(ctx) {
    ctx.os << "<a href='" << params.value_href << "'>";
  }
  ~a() { ctx.os << "</a>"; }
  a& SUERW() { return *this; }
};

}  // namespace ::htmltag

#endif  // BLOCKS_HTML_TAGS_H
