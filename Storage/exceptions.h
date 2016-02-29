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

#ifndef CURRENT_STORAGE_EXCEPTIONS_H
#define CURRENT_STORAGE_EXCEPTIONS_H

#include "../Blocks/GracefulShutdown/exceptions.h"

namespace current {
namespace storage {

struct StorageException : Exception {
  using Exception::Exception;
};

// LCOV_EXCL_START
struct StorageCannotAppendToFileException : StorageException {
  explicit StorageCannotAppendToFileException(const std::string& filename)
      : StorageException("Cannot append to: `" + filename + "`.") {}
};
// LCOV_EXCL_STOP

struct StorageRollbackExceptionWithNoValue : StorageException {
  using StorageException::StorageException;
};

template <typename T>
struct StorageRollbackExceptionWithValue : StorageException {
  StorageRollbackExceptionWithValue(T&& value, const std::string& what = std::string())
      : StorageException(what), value(std::move(value)) {}
  T value;
};

struct StorageInGracefulShutdownException : InGracefulShutdownException {
  using InGracefulShutdownException::InGracefulShutdownException;
};

}  // namespace current::storage
}  // namespace current

#define CURRENT_STORAGE_THROW_ROLLBACK() throw ::current::storage::StorageRollbackExceptionWithNoValue()

#define CURRENT_STORAGE_THROW_ROLLBACK_WITH_VALUE(type, value) \
  throw ::current::storage::StorageRollbackExceptionWithValue<type>(value)

using StorageCannotAppendToFile = const current::storage::StorageCannotAppendToFileException&;
using StorageRollbackExceptionWithNoValue = const current::storage::StorageRollbackExceptionWithNoValue&;
template <typename T>
using StorageRollbackExceptionWithValue = current::storage::StorageRollbackExceptionWithValue<T>&;

#endif  // CURRENT_STORAGE_EXCEPTIONS_H
