/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef CURRENT_STORAGE_TRANSACTION_RESULT_H
#define CURRENT_STORAGE_TRANSACTION_RESULT_H

#include "../TypeSystem/optional_result.h"

namespace current {
namespace storage {

template <typename T>
class TransactionResult : public OptionalResult<T> {
 public:
#ifndef CURRENT_WINDOWS
  TransactionResult() = delete;
#else
  // MSVS needs this for `_Associated_state<>`, required by `std::promise<>`.
  TransactionResult() = default;
#endif
  TransactionResult(const TransactionResult&) = delete;
  TransactionResult& operator=(const TransactionResult&) = delete;

  TransactionResult(TransactionResult&& rhs) : OptionalResult<T>(std::move(rhs)), commited_(rhs.commited_) {}
  TransactionResult& operator=(TransactionResult&& rhs) {
    OptionalResult<T>::operator=(std::move(rhs));
    commited_ = rhs.commited_;
    return *this;
  }

  static TransactionResult<T> Committed(T&& result) { return TransactionResult<T>(std::move(result), true); }

  static TransactionResult<T> Committed(const OptionalResultMissing& missing) {
    return TransactionResult<T>(missing, true);
  }

  static TransactionResult<T> RolledBack(T&& result) { return TransactionResult<T>(std::move(result), false); }

  static TransactionResult<T> RolledBack(const OptionalResultMissing& missing) {
    return TransactionResult<T>(missing, false);
  }

  bool WasCommittedImpl() const { return commited_; }

 private:
  bool commited_;

  TransactionResult(T&& result, bool commited) : OptionalResult<T>(std::move(result)), commited_(commited) {}
  TransactionResult(const OptionalResultMissing& missing, bool commited)
      : OptionalResult<T>(missing), commited_(commited) {}
};

template <>
class TransactionResult<void> : public OptionalResult<void> {
 public:
#ifndef _MSC_VER
  TransactionResult() = delete;
#else
  TransactionResult() = default;  // Required by Visual Studio.
#endif
  TransactionResult(const TransactionResult&) = delete;
  TransactionResult<void>& operator=(const TransactionResult&) = delete;

  TransactionResult(TransactionResult&& rhs) : OptionalResult<void>(std::move(rhs)), commited_(rhs.commited_) {}
  TransactionResult& operator=(TransactionResult&& rhs) {
    OptionalResult<void>::operator=(std::move(rhs));
    commited_ = rhs.commited_;
    return *this;
  }

  static TransactionResult<void> Committed(const OptionalResultExists& exists) {
    return TransactionResult<void>(exists, true);
  }

  static TransactionResult<void> Committed(const OptionalResultMissing& missing) {
    return TransactionResult<void>(missing, true);
  }

  static TransactionResult<void> RolledBack(const OptionalResultExists& exists) {
    return TransactionResult<void>(exists, false);
  }

  // TODO(dkorolev) + TODO(mzhurovich): Test this.
  static TransactionResult<void> RolledBack(const OptionalResultMissing& missing) {
    return TransactionResult<void>(missing, false);
  }

  bool WasCommittedImpl() const { return commited_; }

 private:
  bool commited_;

  TransactionResult(const OptionalResultExists& exists, bool commited)
      : OptionalResult<void>(exists), commited_(commited) {}

  // TODO(dkorolev) + TODO(mzhurovich): Test this.
  TransactionResult(const OptionalResultMissing& missing, bool commited)
      : OptionalResult<void>(missing), commited_(commited) {}
};

template <typename T>
bool WasCommitted(const TransactionResult<T>& x) {
  return x.WasCommittedImpl();
}

template <typename T>
bool WasCommitted(TransactionResult<T>&& x) {
  return x.WasCommittedImpl();
}

}  // namespace storage
}  // namespace current

using current::storage::WasCommitted;

#endif  // CURRENT_STORAGE_TRANSACTION_RESULT_H
