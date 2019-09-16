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

#include <iostream>
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
  std::string current_status_;
  size_t current_status_length_ = 0u;  // W/o VT100 escape sequences. -- D.K.

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
    template <typename T, class = std::enable_if_t<!std::is_base_of_v<vt100::E, current::decay<T>>>>
    Status& operator<<(T&& whatever) {
      oss_text << whatever;
      oss_text_with_no_vt100_escape_sequences << whatever;
      return *this;
    }
  };

  explicit ProgressLine(std::ostream& os = std::cerr) : os_(os) {}
  ~ProgressLine() {
    if (current_status_length_) {
      os_ << ClearString();
    }
  }

  template <typename T>
  Status operator<<(T&& whatever) {
    Status status(*this);
    status << std::forward<T>(whatever);
    return status;
  }

  void DoUpdate(Status& status) {
    std::string new_status = status.oss_text.str();

    if (new_status != current_status_) {
      os_ << ClearString() + new_status << vt100::reset << std::flush;
      current_status_ = new_status;
      current_status_length_ = current::strings::UTF8StringLength(status.oss_text_with_no_vt100_escape_sequences.str());
    }
  }

  std::string ClearString() const {
    const size_t n = current_status_length_;
    return std::string(n, '\b') + std::string(n, ' ') + std::string(n, '\b');
  }
};

}  // namespace current

#endif  // BLOCKS_XTERM_PROGRESS_H
