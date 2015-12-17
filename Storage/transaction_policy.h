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

#ifndef CURRENT_STORAGE_TRANSACTION_POLICY_H
#define CURRENT_STORAGE_TRANSACTION_POLICY_H

#include <mutex>

#include "base.h"

namespace current {
namespace storage {
namespace transaction_policy {

using current::storage::TransactionResult;
using current::storage::MutationJournal;

template <class PERSISTER>
struct Synchronous final {
  Synchronous(PERSISTER& persister, MutationJournal& journal) : persister_(persister), journal_(journal) {}

  template <typename F>
  using T_F_RESULT = typename std::result_of<F()>::type;

  template <typename F, class = ENABLE_IF<!std::is_void<T_F_RESULT<F>>::value>>
  TransactionResult<T_F_RESULT<F>> Transaction(F&& f) {
    std::lock_guard<std::mutex> lock(mutex_);
    journal_.AssertEmpty();
    bool successful = false;
    T_F_RESULT<F> result;
    try {
      result = f();
      successful = true;
    } catch (std::exception&) {
      journal_.Rollback();
    }
    if (successful) {
      persister_.PersistJournal(journal_);
      return TransactionResult<T_F_RESULT<F>>(std::move(result));
    } else {
      return TransactionResult<T_F_RESULT<F>>(current::OptionalResultFailed());
    }
  }

  template <typename F, class = ENABLE_IF<std::is_void<T_F_RESULT<F>>::value>>
  TransactionResult<void> Transaction(F&& f) {
    std::lock_guard<std::mutex> lock(mutex_);
    journal_.AssertEmpty();
    bool successful = false;
    try {
      f();
      successful = true;
    } catch (std::exception&) {
      journal_.Rollback();
    }
    if (successful) {
      persister_.PersistJournal(journal_);
    }
    return TransactionResult<void>(successful);
  }

  // TODO(mz+dk): implement proper logic here (consider rollbacks).
  template <typename F1, typename F2, class = ENABLE_IF<!std::is_void<T_F_RESULT<F1>>::value>>
  void Transaction(F1&& f1, F2&& f2) {
    std::lock_guard<std::mutex> lock(mutex_);
    journal_.AssertEmpty();
    bool successful = false;
    try {
      f2(f1());
      successful = true;
    } catch (std::exception&) {
      journal_.Rollback();
    }
    if (successful) {
      persister_.PersistJournal(journal_);
    }
  }

 private:
  PERSISTER& persister_;
  MutationJournal& journal_;
  std::mutex mutex_;
};

}  // namespace transaction_policy
}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_TRANSACTION_POLICY_H
