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

#ifndef BLOCKS_SS_EXCEPTIONS_H
#define BLOCKS_SS_EXCEPTIONS_H

#include "../../port.h"
#include "../../Bricks/exception.h"

#include <chrono>

namespace current {
namespace ss {

struct InconsistentIndexOrTimestampException : Exception {
  using Exception::Exception;
};

struct InconsistentIndexException : InconsistentIndexOrTimestampException {
  using InconsistentIndexOrTimestampException::InconsistentIndexOrTimestampException;
  InconsistentIndexException(uint64_t expected, uint64_t found) {
    using current::strings::Printf;
    SetWhat(Printf("Expecting index %llu, seeing %llu.", expected, found));
  }
};

struct InconsistentTimestampException : InconsistentIndexOrTimestampException {
  using InconsistentIndexOrTimestampException::InconsistentIndexOrTimestampException;
  InconsistentTimestampException(std::chrono::microseconds expected, std::chrono::microseconds found) {
    using current::strings::Printf;
    SetWhat(Printf("Expecting timestamp >= %llu, seeing %llu.", expected.count(), found.count()));
  }
};

}  // namespace current::ss
}  // namespace current

#endif  // BLOCKS_SS_EXCEPTIONS_H
