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

#ifndef BLOCKS_HTML_HTML_H
#define BLOCKS_HTML_HTML_H

// NOTE(dkorolev): This header file pollutes the global "namespace" with a macro called "HTML".
//                 You probably don't want to `#include` it until you know exactly what you're doing.

#include "../../port.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../../bricks/exception.h"
#include "../../bricks/util/singleton.h"

namespace current {
namespace html {

// An exception that is thrown if multiple interesting HTML generation scopes per thread are attempted to be used.
struct HTMLGenerationIntersectingScopesException : Exception {
  using Exception::Exception;
};

// An exception that is thrown if no HTML generation scope was initialized for this thread.
struct HTMLGenerationNoActiveScopeException : Exception {
  const char* tag;
  const char* file;
  const int line;
  HTMLGenerationNoActiveScopeException(const char* tag, const char* file, const int line)
      : tag(tag), file(file), line(line) {}
};

struct HTMLGeneratorScope {
  virtual ~HTMLGeneratorScope() = default;
  virtual std::ostream& OutputStream() = 0;
};

// The thread-local singleton to manage the context.
class HTMLGeneratorThreadLocalSingleton final {
 private:
  HTMLGeneratorScope* scope_ptr_ = nullptr;

 public:
  struct CallReferencingFileLine {
    HTMLGeneratorThreadLocalSingleton& self;
    const char* file;
    const int line;
    CallReferencingFileLine(HTMLGeneratorThreadLocalSingleton& self, const char* file, const int line)
        : self(self), file(file), line(line) {}
  };

  CallReferencingFileLine Call(const char* file, int line) { return CallReferencingFileLine(*this, file, line); }

  bool HasActiveScope() const { return scope_ptr_ != nullptr; }

  HTMLGeneratorScope& ActiveScope(const char* tag_name, const char* file, int line) {
    if (scope_ptr_) {
      return *scope_ptr_;
    } else {
      CURRENT_THROW(HTMLGenerationNoActiveScopeException(tag_name, file, line));
    }
  }

  void RegisterSelfAsScope(HTMLGeneratorScope* scope) {
    if (scope_ptr_) {
      CURRENT_THROW(HTMLGenerationIntersectingScopesException());
    } else {
      scope_ptr_ = scope;
    }
  }

  void UnregisterSelfAsScope(HTMLGeneratorScope* scope) {
    if (scope_ptr_ == scope) {
      scope_ptr_ = nullptr;
    } else {
      std::cerr << "INTERNAL ERROR: An inactive HTML generation scope attempted to unregister itself; ignored.";
    }
  }
};

struct HTMLGeneratorOStreamScope final : HTMLGeneratorScope {
  std::ostream& os;
  explicit HTMLGeneratorOStreamScope(std::ostream& os) : os(os) {
    ThreadLocalSingleton<HTMLGeneratorThreadLocalSingleton>().RegisterSelfAsScope(this);
  }
  ~HTMLGeneratorOStreamScope() {
    ThreadLocalSingleton<HTMLGeneratorThreadLocalSingleton>().UnregisterSelfAsScope(this);
  }
  std::ostream& OutputStream() override { return os; }
};

// C-style macros magic. Don't try to understand it. Thanks.
#define CURRENT_HTML_CONCAT(x, y) x##y
#define CURRENT_HTML_ID_PREFIX(z) CURRENT_HTML_CONCAT(__htmlnode_, z)
#define CURRENT_HTML_ID CURRENT_HTML_ID_PREFIX(__LINE__)

// NOTE(dkorolev): `SUERW()`, which just returns `*this`, stands for `SuppressUnusedExpressionResultWarning`.
#define CURRENT_HTML_COUNT_1(TAG)                                                                        \
  auto CURRENT_HTML_ID = ::htmltag::TAG(                                                                 \
      ::current::ThreadLocalSingleton<::current::html::HTMLGeneratorThreadLocalSingleton>().ActiveScope( \
          #TAG, __FILE__, __LINE__),                                                                     \
      __FILE__,                                                                                          \
      __LINE__);                                                                                         \
  CURRENT_HTML_ID.SUERW()

#define CURRENT_HTML_COUNT_2(TAG, ARG)                                                                   \
  auto CURRENT_HTML_ID = ::htmltag::TAG(                                                                 \
      ::current::ThreadLocalSingleton<::current::html::HTMLGeneratorThreadLocalSingleton>().ActiveScope( \
          #TAG, __FILE__, __LINE__),                                                                     \
      __FILE__,                                                                                          \
      __LINE__,                                                                                          \
      ::htmltag::TAG::Params().ARG);                                                                     \
  CURRENT_HTML_ID.SUERW()

#define CURRENT_HTML_NARGS_IMPL(_1, _2, n, ...) n
#define CURRENT_HTML_NARGS_IMPL_CALLER(args) CURRENT_HTML_NARGS_IMPL args

#define CURRENT_HTML_NARGS(...) CURRENT_HTML_NARGS_IMPL_CALLER((__VA_ARGS__, 2, 1, 0))

#define CURRENT_HTML_CASE_3(n) CURRENT_HTML_COUNT_##n
#define CURRENT_HTML_CASE_2(n) CURRENT_HTML_CASE_3(n)
#define CURRENT_HTML_CASE_1(n) CURRENT_HTML_CASE_2(n)
#define CURRENT_HTML_SWITCH_N(n) CURRENT_HTML_CASE_1(n)

#define CURRENT_HTML_SWITCH(x, y) x y
#define HTML(...) CURRENT_HTML_SWITCH(CURRENT_HTML_SWITCH_N(CURRENT_HTML_NARGS(__VA_ARGS__)), (__VA_ARGS__))

}  // namespace current::html
}  // namespace current

// The `htmltag` namespace is intentionally in the global scope, not within `::current`, so that it can be amended to.
namespace htmltag {

// This header file only defines the `_` tag, which stands for raw HTML body. Refer to the `tags.h` header for more.
struct _ final {
  std::ostream& os;
  _(::current::html::HTMLGeneratorScope& scope, const char*, int) : os(scope.OutputStream()) {}
  _& SUERW() { return *this; }
  template <typename ARG>
  _& operator<<(ARG&& arg) {
    os << std::forward<ARG>(arg);
    return *this;
  }
};

}  // namespace ::htmltag

#include "tags.h"

#endif  // BLOCKS_HTML_HTML_H
