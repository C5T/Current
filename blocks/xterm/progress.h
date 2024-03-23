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

#ifndef BLOCKS_XTERM_PROGRESS_H
#define BLOCKS_XTERM_PROGRESS_H

#include <functional>
#include <iostream>
#include <mutex>
#include <sstream>
#include <type_traits>

#include "../../bricks/strings/util.h"
#include "../../bricks/template/decay.h"
#include "../../port.h"

#include "vt100.h"

namespace current {

class ProgressLine final {
 private:
  std::ostream& os_;
  std::function<int()> up_;
  std::mutex* maybe_mutex_;
  std::string current_status_;
  std::string current_undecorated_status_;
  size_t current_status_length_ = 0u;  // W/o VT100 escape sequences. -- D.K.

  std::unique_lock<std::mutex> MaybeLock() {
    return maybe_mutex_ ? std::unique_lock<std::mutex>(*maybe_mutex_) : std::unique_lock<std::mutex>();
  }

  void DoClearString(std::ostream& os) const {
    const size_t n = current_status_length_;
    std::string back(n, '\b');
    std::string forward(n, ' ');
    os << back << forward << back;
  }

  // A helper private constructor to accept a reference, not a pointer, to the (optional, member pointer to a) mutex.
  struct DoConstruct final {};
  ProgressLine(DoConstruct,
               std::ostream& os = std::cout,
               std::function<int()> up = nullptr,
               std::mutex* maybe_mutex = nullptr)
      : os_(os), up_(up), maybe_mutex_(maybe_mutex) {
    if (up_) {
      // Move to the next line right away to "allocate" a new line for this "multiline" "progress instance".
      const auto maybe_lock = MaybeLock();
      os_ << '\n';
    }
  }

 public:
  // Need to keep track of VT100 escape sequences to not `\b`-erase them.
  // UTF8 characters seem to be doing just fine, I checked. -- D.K.
  struct Status final {
    ProgressLine& self;
    std::ostringstream oss_text;
    std::ostringstream oss_text_with_no_vt100_escape_sequences;
    bool active = true;

    explicit Status(ProgressLine& self) : self(self) {}
    Status(Status&& status) : self(status.self) {
      oss_text << status.oss_text.str();
      oss_text_with_no_vt100_escape_sequences << status.oss_text_with_no_vt100_escape_sequences.str();
      status.active = false;
    }

    ~Status() {
      if (active) {
        self.DoUpdate(*this);
      }
    }

    Status& operator<<(const vt100::E& escape_sequence) {
      oss_text << escape_sequence;
      return *this;
    }

    // The `std::enable_if_t` is necessary, as well as `current::decay`, because of `vt100::Color`. -- D.K.
    template <typename T, class = std::enable_if_t<!std::is_base_of_v<vt100::E, current::decay_t<T>>>>
    Status& operator<<(T&& whatever) {
      oss_text << whatever;
      oss_text_with_no_vt100_escape_sequences << whatever;
      return *this;
    }
  };

  ProgressLine(std::ostream& os = std::cout, std::function<int()> up = nullptr) : ProgressLine(DoConstruct(), os, up) {}
  ProgressLine(std::ostream& os, std::function<int()> up, std::mutex& mx) : ProgressLine(DoConstruct(), os, up, &mx) {}

  ~ProgressLine() {
    if (!up_) {
      if (current_status_length_) {
        const auto maybe_lock = MaybeLock();
        DoClearString(os_);
      }
    }
    // In multiline mode, keep the last string that was there.
  }

  template <typename T>
  Status operator<<(T&& whatever) {
    Status status(*this);
    status << std::forward<T>(whatever);
    return status;
  }

  std::string GetUndecoratedString() const { return current_undecorated_status_; }

 protected:
  void DoUpdate(Status& status) {
    std::string new_status = status.oss_text.str();
    current_undecorated_status_ = status.oss_text_with_no_vt100_escape_sequences.str();

    if (new_status != current_status_) {
      const size_t new_status_length =
          current::strings::UTF8StringLength(status.oss_text_with_no_vt100_escape_sequences.str());
      if (!up_) {
        const auto maybe_lock = MaybeLock();
        DoClearString(os_);
        os_ << new_status << vt100::reset << std::flush;
      } else {
        // When this progress line may not be the top one, always add `std::endl` at the end,
        // so that the caret is on the leftmost character.
        const int n = up_();
        const auto maybe_lock = MaybeLock();
        if (new_status_length > current_status_length_) {
          os_ << vt100::up(n + 1) << new_status << vt100::reset << '\n' << vt100::down(n) << std::flush;
        } else {
          const size_t d = current_status_length_ - new_status_length;
          os_ << vt100::up(n + 1) << new_status << vt100::reset << std::string(d, ' ') << '\n'
              << vt100::down(n) << std::flush;
        }
      }
      current_status_ = new_status;
      current_status_length_ = new_status_length;
    }
  }
};

class MultilineProgress final {
 private:
  int total_ = 0u;
  std::mutex mutex_;

 public:
  current::ProgressLine operator()(std::ostream& os = std::cout) {
    ++total_;
    int index = total_;
    return current::ProgressLine(
        os, [index, this]() { return total_ - index; }, mutex_);
  }
  std::unique_ptr<current::ProgressLine> UniqueProgressLine(std::ostream& os = std::cout) {
    ++total_;
    int index = total_;
    return std::make_unique<current::ProgressLine>(
        os, [index, this]() { return total_ - index; }, mutex_);
  }
};

}  // namespace current

#endif  // BLOCKS_XTERM_PROGRESS_H
