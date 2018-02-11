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

  HTMLGeneratingContext() { errors.push_back("HTML generating context uninitialized."); }

  void Reset() {
    os.clear();
    errors.clear();
  }

  void PartialReset() {
    // Wipe the current contents just in case there was any sensitive data already.
    // It won't go out regardless, but just in case. -- D.K.
    os.clear();
  }
};

// The thread-local singleton to manage the context.
class HTMLGenerator {
 private:
  bool started = false;
  HTMLGeneratingContext context;

 public:
  void BeginHTML(const char* file, int line) {
    if (started) {
      context.PartialReset();
      std::ostringstream error;
      error << "Attempted to call X without Y on ";
#if 1
      error << file << ':';
#else
      error << "UNITTEST:";
#endif
      error << line;
      context.errors.push_back(error.str());
    } else {
      context.Reset();
      started = true;
    }
  }

  std::string EndHTML(const char* file, int line) {
    if (!started) {
      std::ostringstream error;
      error << "Attempted to call Y without X on ";
#if 1
      error << file << ':';
#else
      error << "UNITTEST:";
#endif
      error << line;
      context.errors.push_back(error.str());

      std::ostringstream result;
      for (const std::string& error : context.errors) {
        result << error << '\n';
      }
      return result.str();
    } else {
      started = false;
      return context.os.str();
    }
  }

  HTMLGeneratingContext& Ctx(const char* tag_name, const char* file, int line) {
    if (!started) {
      context.Reset();
      std::ostringstream error;
      error << "Forgot to call X before doing HTML(" << tag_name << ") on ";
#if 1
      error << file << ':';
#else
      error << "UNITTEST:";
#endif
      error << line;
      context.errors.push_back(error.str());
    }
    static_cast<void>(tag_name);
    return context;
  }
};

#define CURRENT_HTML_CONCAT(x, y) x##y
#define CURRENT_HTML_ID_PREFIX(z) CURRENT_HTML_CONCAT(__htmlnode_, z)
#define CURRENT_HTML_ID CURRENT_HTML_ID_PREFIX(__LINE__)

// NOTE(dkorolev): `SUERW()`, which just returns `*this`, stands for `SuppressUnusedExpressionResultWarning`.
#define HTML(TAG)                                                                                                     \
  auto CURRENT_HTML_ID =                                                                                              \
      ::htmltag::TAG(::current::ThreadLocalSingleton<::current::html::HTMLGenerator>().Ctx(#TAG, __FILE__, __LINE__), \
                     __FILE__,                                                                                        \
                     __LINE__);                                                                                       \
  CURRENT_HTML_ID.SUERW()

}  // namespace current::html
}  // namespace current

// The `htmltag` namespace is intentionally in the global scope, not within `::current`.
namespace htmltag {

struct UnsafeText {
  ::current::html::HTMLGeneratingContext& ctx;
  UnsafeText(::current::html::HTMLGeneratingContext& ctx, const char*, int) : ctx(ctx) {}
  UnsafeText& SUERW() { return *this; }
  template <typename ARG>
  UnsafeText& operator<<(ARG&& arg) {
    ctx.os << std::forward<ARG>(arg);
    return *this;
  }
};

}  // namespace ::htmltag

#endif  // BLOCKS_HTML_HTML_H
