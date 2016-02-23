/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPREPERSISTENCE OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNEPERSISTENCE FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

#ifndef BLOCKS_PERSISTENCE_EXCEPTIONS_H
#define BLOCKS_PERSISTENCE_EXCEPTIONS_H

#include <chrono>

#include "../../Bricks/exception.h"
#include "../../Bricks/strings/printf.h"

namespace current {
namespace persistence {

struct PersistenceException : current::Exception {
  using current::Exception::Exception;
};

struct MalformedEntryException : PersistenceException {
  using PersistenceException::PersistenceException;
};

struct InconsistentIndexOrTimestampException : PersistenceException {
  using PersistenceException::PersistenceException;
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

struct InvalidIterableRangeException : PersistenceException {
  using PersistenceException::PersistenceException;
};

}  // namespace peristence
}  // namespace current

#endif  // BLOCKS_PERSISTENCE_EXCEPTIONS_H
