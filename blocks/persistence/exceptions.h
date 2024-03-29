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

#include "../graceful_shutdown/exceptions.h"

#include "../../bricks/strings/printf.h"

namespace current {
namespace persistence {

struct PersistenceException : current::Exception {
  using current::Exception::Exception;
};

struct MalformedEntryException : PersistenceException {
  using PersistenceException::PersistenceException;
};

struct InvalidIterableRangeException : PersistenceException {
  using PersistenceException::PersistenceException;
};

struct NoEntriesPublishedYet : PersistenceException {
  using PersistenceException::PersistenceException;
};

struct InvalidStreamSignature : PersistenceException {
  explicit InvalidStreamSignature(const std::string& expected, const std::string& actual)
      : PersistenceException("Invalid signature,\nexpected:\n`" + expected + "`\nactual:\n`" + actual + "`.") {}
};

struct InvalidSignatureLocation : PersistenceException {
  using PersistenceException::PersistenceException;
};

struct PersistenceFileNotWritable : PersistenceException {
  explicit PersistenceFileNotWritable(const std::string& filename)
      : PersistenceException("Persistence file not writable: `" + filename + "`.") {}
};

struct UnsafePublishBadIndexTimestampException : PersistenceException {
  explicit UnsafePublishBadIndexTimestampException(uint64_t expected, uint64_t found)
      : PersistenceException(current::strings::Printf(
            "Expecting index %lld, seeing %lld.", static_cast<long long>(expected), static_cast<long long>(found))) {}
};

}  // namespace persistence
}  // namespace current

#endif  // BLOCKS_PERSISTENCE_EXCEPTIONS_H
