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

#include <sstream>
#include <string>
#include <vector>

#include "../../bricks/exception.h"
#include "../../bricks/util/singleton.h"

namespace current {
namespace html {

// The lightweight context to be passed to the callers.
struct HTMLGeneratingContext {
  std::ostringstream os;
  std::vector<std::string> errors;

  HTMLGeneratingContext() { FullReset(); }

  void FullReset() {
    std::ostringstream().swap(os);
    errors.clear();
  }

#ifdef CURRENT_HTML_UNIT_TEST
  void ResetForUnitTest() { FullReset(); }
#endif

  void Reset() {
    std::ostringstream().swap(os);
    errors.clear();
  }

  void PartialReset() {
    // Wipe the current contents just in case there was any sensitive data already.
    // It won't go out regardless, but just in case. -- D.K.
    std::ostringstream().swap(os);
  }
};

// The thread-local singleton to manage the context.
class HTMLGeneratorThreadLocalSingleton {
 private:
  bool initialized = false;
  bool started = false;
  HTMLGeneratingContext context;

 public:
  struct CallReferencingFileLine {
    HTMLGeneratorThreadLocalSingleton& self;
    const char* file;
    const int line;
    CallReferencingFileLine(HTMLGeneratorThreadLocalSingleton& self, const char* file, const int line)
        : self(self), file(file), line(line) {}

#ifdef CURRENT_HTML_UNIT_TEST
    void ResetForUnitTest() {
      self.initialized = false;
      self.started = false;
      self.context.ResetForUnitTest();
    }
#endif

    void Begin() {
      if (self.started) {
        self.context.PartialReset();
        std::ostringstream error;
        error << "Attempted to call HTMLGenerator.Begin() more than once in a row @ ";
#ifndef CURRENT_HTML_UNIT_TEST  // Only dump file names in non-unittest builds.
        error << file << ':';
#else
        error << "UNITTEST:";
#endif
        error << line;
        self.context.errors.push_back(error.str());
      } else {
        self.context.Reset();
        self.started = true;
        self.initialized = true;
      }
    }

    std::string End() {
      if (!self.initialized || !self.started || !self.context.errors.empty()) {
        std::ostringstream error;
        if (!self.initialized) {
          error << "Attempted to call HTMLGenerator.End() on an uninitialized HTMLGenerator @ ";
        } else if (!self.started) {
          error << "Attempted to call HTMLGenerator.End() more than once in a row @ ";
        } else {
          error << "Attempted to call HTMLGenerator.End() with critical errors @ ";
        }
#ifndef CURRENT_HTML_UNIT_TEST  // Only dump file names in non-unittest builds.
        error << file << ':';
#else
        error << "UNITTEST:";
#endif
        error << line;
        self.context.errors.push_back(error.str());

        std::ostringstream result;
        for (const std::string& error : self.context.errors) {
          result << error << '\n';
        }
        return result.str();
      } else {
        self.started = false;
        return self.context.os.str();
      }
    }
  };

  CallReferencingFileLine Call(const char* file, int line) { return CallReferencingFileLine(*this, file, line); }

  HTMLGeneratingContext& Ctx(const char* tag_name, const char* file, int line) {
    if (!started) {
      context.Reset();
      std::ostringstream error;
      error << "Forgot to call X before doing HTML(" << tag_name << ") on ";
#ifndef CURRENT_HTML_UNIT_TEST  // Only dump file names in non-unittest builds.
      error << file << ':';
#else
      static_cast<void>(file);
      error << "UNITTEST:";
#endif
      error << line;
      context.errors.push_back(error.str());
    }
    static_cast<void>(tag_name);
    return context;
  }
};

// C-style macros magic. Don't try to understand it. Thanks.
#define CURRENT_HTML_CONCAT(x, y) x##y
#define CURRENT_HTML_ID_PREFIX(z) CURRENT_HTML_CONCAT(__htmlnode_, z)
#define CURRENT_HTML_ID CURRENT_HTML_ID_PREFIX(__LINE__)

// NOTE(dkorolev): `SUERW()`, which just returns `*this`, stands for `SuppressUnusedExpressionResultWarning`.
#define CURRENT_HTML_COUNT_1(TAG)                                                                               \
  auto CURRENT_HTML_ID =                                                                                        \
      ::htmltag::TAG(::current::ThreadLocalSingleton<::current::html::HTMLGeneratorThreadLocalSingleton>().Ctx( \
                         #TAG, __FILE__, __LINE__),                                                             \
                     __FILE__,                                                                                  \
                     __LINE__);                                                                                 \
  CURRENT_HTML_ID.SUERW()

#define CURRENT_HTML_COUNT_2(TAG, ARG)                                                                          \
  auto CURRENT_HTML_ID =                                                                                        \
      ::htmltag::TAG(::current::ThreadLocalSingleton<::current::html::HTMLGeneratorThreadLocalSingleton>().Ctx( \
                         #TAG, __FILE__, __LINE__),                                                             \
                     __FILE__,                                                                                  \
                     __LINE__,                                                                                  \
                     ::htmltag::TAG::Params().ARG);                                                             \
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

#define HTMLGenerator \
  ::current::ThreadLocalSingleton<::current::html::HTMLGeneratorThreadLocalSingleton>().Call(__FILE__, __LINE__)

}  // namespace current::html
}  // namespace current

// The `htmltag` namespace is intentionally in the global scope, not within `::current`, so that it can be amended to.
namespace htmltag {

struct _ final {
  ::current::html::HTMLGeneratingContext& ctx;
  _(::current::html::HTMLGeneratingContext& ctx, const char*, int) : ctx(ctx) {}
  _& SUERW() { return *this; }
  template <typename ARG>
  _& operator<<(ARG&& arg) {
    ctx.os << std::forward<ARG>(arg);
    return *this;
  }
};

}  // namespace ::htmltag

#include "tags.h"

#endif  // BLOCKS_HTML_HTML_H
