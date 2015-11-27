/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef SHERLOCK_EXTRACT_TIMESTAMP_H
#define SHERLOCK_EXTRACT_TIMESTAMP_H

#include "../port.h"

#include "../Bricks/time/chrono.h"
#include "../Bricks/template/is_unique_ptr.h"

namespace sherlock {

template <bool IS_UNIQUE_PTR>
struct ExtractTimestampImpl {};

template <>
struct ExtractTimestampImpl<false> {
  template <typename E>
  static EpochMicroseconds DoIt(E&& e) {
    return e.ExtractTimestamp();
  }
};

template <>
struct ExtractTimestampImpl<true> {
  template <typename E>
  static EpochMicroseconds DoIt(E&& e) {
    return e->ExtractTimestamp();
  }
};

template <typename E>
EpochMicroseconds ExtractTimestamp(E&& entry) {
  return ExtractTimestampImpl<current::is_unique_ptr<E>::value>::template DoIt<E>(std::forward<E>(entry));
}

}  // namespace sherlock

#endif  // SHERLOCK_EXTRACT_TIMESTAMP_H
