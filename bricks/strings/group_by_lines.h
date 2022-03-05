/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2020 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_STRINGS_GROUP_BY_LINES_H
#define BRICKS_STRINGS_GROUP_BY_LINES_H

#include "../../port.h"
#include "../exception.h"
#include "../util/singleton.h"

#ifdef CURRENT_FOR_CPP14
#include "../template/weed.h"
#endif  // CURRENT_FOR_CPP14

#include <deque>
#include <functional>
#include <string>
#include <type_traits>
#include <iostream>

namespace current {
namespace strings {

struct GenericStatefulGroupByLinesProcessCPPString final {
  template <class F>
  static void DoProcess(std::string&& s, F&& f) {
    f(std::move(s));
  }
};

struct GenericStatefulGroupByLinesProcessCString final {
  template <class F>
  static void DoProcess(std::string&& s, F&& f) {
    f(s.c_str());
  }
};

class ExceptionFriendlyStatefulGroupByLinesDoneNotCalledHandler final {
 private:
  static void DefaultHandler() {
    std::cerr << "Contract failure: no `.Done()` called on an exception-friendly `GroupByLines`." << std::endl;
    std::exit(-1);
  }
  std::function<void()> handler_ = DefaultHandler;

 public:
  void InjectHandler(std::function<void()> f) { handler_ = f; }
  void ResetHandler() { handler_ = DefaultHandler; }
  void Signal() { handler_(); }
};

enum class GroupByLinesExceptions : bool { Prohibit = false, Allow = true };

struct GroupByLinesFeedCaledAfterDone final : Exception {};
struct GroupByLinesDoneCalledTwice final : Exception {};

template <class F, GroupByLinesExceptions E, class PROCESSOR>
class GenericStatefulGroupByLines final {
 private:
  F f_;
  std::string residual_;
  bool exception_thrown_ = false;
  bool done_called_ = false;

  void DoDone() {
    if (!residual_.empty()) {
      std::string extracted;
      residual_.swap(extracted);
      PROCESSOR::DoProcess(std::move(extracted), f_);
    }
  }

 public:
  explicit GenericStatefulGroupByLines(F&& f) : f_(std::move(f)) {}
  void Feed(const std::string& s) { Feed(s.c_str()); }
  void Feed(const char* s) {
    if (done_called_) {
      CURRENT_THROW(GroupByLinesFeedCaledAfterDone());
    }
    if (exception_thrown_) {
      // Silently ignore further input if the user code has thrown an exception before.
      return;
    }
    try {
      // NOTE: `Feed`() will only call the callback upon seeing the newline in the input data block.
      // If the last line does not end with a newline, it will not be forwarded until either
      // the `GenericStatefulGroupByLines` instance is destroyed (in the `E == GroupByLinesExceptions::Prohibit` mode),
      // or the `.Done()` method is called (in the `GroupByLinesExceptions::Allow` mode).
      while (true) {
        while (*s && *s != '\n') {
          residual_ += *s++;
        }
        if (*s == '\n') {
          ++s;
          std::string extracted;
          residual_.swap(extracted);
          PROCESSOR::DoProcess(std::move(extracted), f_);
        } else {
          break;
        }
      }
    } catch (...) {
      exception_thrown_ = true;
      throw;
    }
  }

  template <bool ENABLE = (E == GroupByLinesExceptions::Allow)>
  std::enable_if_t<ENABLE> Done() {
    if (done_called_) {
      CURRENT_THROW(GroupByLinesDoneCalledTwice());
    }
    done_called_ = true;
    DoDone();
  }

  template <bool ENABLE = (E == GroupByLinesExceptions::Allow)>
  std::enable_if_t<ENABLE, bool> DebugWasDoneCalled() {
    return done_called_;
  }

  ~GenericStatefulGroupByLines() {
#ifndef CURRENT_FOR_CPP14
    if constexpr (E == GroupByLinesExceptions::Prohibit) {
#else
    if (E == GroupByLinesExceptions::Prohibit) {
#endif  // CURRENT_FOR_CPP14
      // Upon destruction, process the the last incomplete line, if necessary.
      DoDone();
    } else {
      // In exception-friendly mode make sure `Done()` was called before.
      if (!done_called_) {
        current::Singleton<ExceptionFriendlyStatefulGroupByLinesDoneNotCalledHandler>().Signal();
      }
    }
  }
};

using StatefulGroupByLines = GenericStatefulGroupByLines<std::function<void(std::string&&)>,
                                                         GroupByLinesExceptions::Prohibit,
                                                         GenericStatefulGroupByLinesProcessCPPString>;
using StatefulGroupByLinesCStringProcessing = GenericStatefulGroupByLines<std::function<void(const char*)>,
                                                                          GroupByLinesExceptions::Prohibit,
                                                                          GenericStatefulGroupByLinesProcessCString>;

using ExceptionFriendlyStatefulGroupByLines = GenericStatefulGroupByLines<std::function<void(std::string&&)>,
                                                                          GroupByLinesExceptions::Allow,
                                                                          GenericStatefulGroupByLinesProcessCPPString>;
using ExceptionFriendlyStatefulGroupByLinesCStringProcessing =
    GenericStatefulGroupByLines<std::function<void(const char*)>,
                                GroupByLinesExceptions::Allow,
                                GenericStatefulGroupByLinesProcessCString>;

template <class F, GroupByLinesExceptions E, bool CPP_STRING, bool C_STRING>
struct CreateStatefulGroupByLinesImpl;

template <class F, GroupByLinesExceptions E>
struct CreateStatefulGroupByLinesImpl<F, E, true, true> final {
  using type_t =
      GenericStatefulGroupByLines<std::function<void(std::string&&)>, E, GenericStatefulGroupByLinesProcessCPPString>;
  static type_t DoIt(F&& f) { return type_t(std::forward<F>(f)); }
};

template <class F, GroupByLinesExceptions E, bool B>
struct CreateStatefulGroupByLinesImpl<F, E, false, B> final {
  using type_t =
      GenericStatefulGroupByLines<std::function<void(const char*)>, E, GenericStatefulGroupByLinesProcessCString>;
  static type_t DoIt(F&& f) { return type_t(std::forward<F>(f)); }
};

#ifndef CURRENT_FOR_CPP14
template <class F>
auto CreateStatefulGroupByLines(F&& f) {
  return CreateStatefulGroupByLinesImpl<F,
                                        GroupByLinesExceptions::Prohibit,
                                        std::is_invocable<F, std::string&&>::value,
                                        std::is_invocable<F, const char*>::value>::DoIt(std::forward<F>(f));
}

template <class F>
auto CreateExceptionFriendlyStatefulGroupByLines(F&& f) {
  return CreateStatefulGroupByLinesImpl<F,
                                        GroupByLinesExceptions::Allow,
                                        std::is_invocable<F, std::string&&>::value,
                                        std::is_invocable<F, const char*>::value>::DoIt(std::forward<F>(f));
}
#else
template <class F>
auto CreateStatefulGroupByLines(F&& f) {
  return CreateStatefulGroupByLinesImpl<
      F,
      GroupByLinesExceptions::Prohibit,
      current::weed::call_with<F, std::string&&>::implemented,
      current::weed::call_with<F, const char*>::implemented>::DoIt(std::forward<F>(f));
}

template <class F>
auto CreateExceptionFriendlyStatefulGroupByLines(F&& f) {
  return CreateStatefulGroupByLinesImpl<
      F,
      GroupByLinesExceptions::Allow,
      current::weed::call_with<F, std::string&&>::implemented,
      current::weed::call_with<F, const char*>::implemented>::DoIt(std::forward<F>(f));
}
#endif  // CURRENT_FOR_CPP14

}  // namespace strings
}  // namespace current

#endif  // BRICKS_STRINGS_GROUP_BY_LINES_H
