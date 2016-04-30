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

#ifndef BRICKS_STRINGS_TIME_H
#define BRICKS_STRINGS_TIME_H

#include "util.h"

namespace current {
namespace strings {

inline std::string TimeIntervalAsHumanReadableString(std::chrono::microseconds us) {
  size_t seconds = static_cast<size_t>(us.count() * 1e-6);
  if (seconds < 60) {
    return ToString(seconds) + 's';
  } else {
    size_t minutes = seconds / 60;
    seconds %= 60;
    if (minutes < 60) {
      return ToString(minutes) + "m " + ToString(seconds) + 's';
    } else {
      size_t hours = minutes / 60;
      minutes %= 60;
      if (hours < 24) {
        return ToString(hours) + "h " + ToString(minutes) + "m " + ToString(seconds) + 's';
      } else {
        const size_t days = hours / 24;
        hours %= 24;
        return ToString(days) + "d " + ToString(hours) + "h " + ToString(minutes) + "m " + ToString(seconds) +
               's';
      }
    }
  }
}

}  // namespace strings
}  // namespace current

#endif  // BRICKS_STRINGS_TIME_H
