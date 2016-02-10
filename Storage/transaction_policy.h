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
#include "exceptions.h"
#include "transaction_result.h"

#include "../Bricks/util/future.h"

namespace current {
namespace storage {
namespace transaction_policy {

template <class PERSISTER>
struct Synchronous final {
  Synchronous(PERSISTER& persister, MutationJournal& journal) : persister_(persister), journal_(journal) {}

  ~Synchronous() {
    std::lock_guard<std::mutex> lock(mutex_);
    destructing_ = true;
  }

  template <typename F>
  using T_F_RESULT = typename std::result_of<F()>::type;

  template <typename F, class = ENABLE_IF<!std::is_void<T_F_RESULT<F>>::value>>
  Future<TransactionResult<T_F_RESULT<F>>, StrictFuture::Strict> Transaction(F&& f) {
    using T_RESULT = T_F_RESULT<F>;
    std::lock_guard<std::mutex> lock(mutex_);
    journal_.AssertEmpty();
    std::promise<TransactionResult<T_RESULT>> promise;
    Future<TransactionResult<T_RESULT>, StrictFuture::Strict> future = promise.get_future();
    if (destructing_) {
      promise.set_exception(std::make_exception_ptr(StorageIsDestructingException()));
      return future;
    }
    bool successful = false;
    T_RESULT f_result;
    try {
      f_result = f();
      successful = true;
    } catch (StorageRollbackExceptionWithValue<T_RESULT> e) {
      journal_.Rollback();
      promise.set_value(TransactionResult<T_RESULT>::Rollbacked(std::move(e.value)));
    } catch (StorageRollbackExceptionWithNoValue) {
      journal_.Rollback();
      promise.set_value(TransactionResult<T_RESULT>::Rollbacked(OptionalResultMissing()));
    } catch (...) {  // The exception is captured with `std::current_exception()` below.
      journal_.Rollback();
      try {
        promise.set_exception(std::current_exception());
      } catch (const std::exception& e) {
        std::cerr << "Storage internal error in Synchronous::Transaction: " << e.what() << std::endl;
        std::exit(-1);
      }
    }
    if (successful) {
      persister_.PersistJournal(journal_);
      promise.set_value(TransactionResult<T_RESULT>::Commited(std::move(f_result)));
    }
    return future;
  }

  template <typename F, class = ENABLE_IF<std::is_void<T_F_RESULT<F>>::value>>
  Future<TransactionResult<void>, StrictFuture::Strict> Transaction(F&& f) {
    std::lock_guard<std::mutex> lock(mutex_);
    journal_.AssertEmpty();
    std::promise<TransactionResult<void>> promise;
    Future<TransactionResult<void>, StrictFuture::Strict> future = promise.get_future();
    if (destructing_) {
      promise.set_exception(std::make_exception_ptr(StorageIsDestructingException()));
      return future;
    }
    bool successful = false;
    try {
      f();
      successful = true;
    } catch (StorageRollbackExceptionWithNoValue) {
      journal_.Rollback();
      promise.set_value(TransactionResult<void>::Rollbacked(OptionalResultExists()));
    } catch (...) {  // The exception is captured with `std::current_exception()` below.
      journal_.Rollback();
      try {
        promise.set_exception(std::current_exception());
      } catch (const std::exception& e) {
        std::cerr << "Storage internal error in Synchronous::Transaction: " << e.what() << std::endl;
        std::exit(-1);
      }
    }
    if (successful) {
      persister_.PersistJournal(journal_);
      promise.set_value(TransactionResult<void>::Commited(OptionalResultExists()));
    }
    return future;
  }

  // TODO(mz+dk): implement proper logic here (consider rollbacks & exceptions).
  template <typename F1, typename F2, class = ENABLE_IF<!std::is_void<T_F_RESULT<F1>>::value>>
  Future<TransactionResult<void>, StrictFuture::Strict> Transaction(F1&& f1, F2&& f2) {
    std::lock_guard<std::mutex> lock(mutex_);
    journal_.AssertEmpty();
    std::promise<TransactionResult<void>> promise;
    Future<TransactionResult<void>, StrictFuture::Strict> future = promise.get_future();
    if (destructing_) {
      promise.set_exception(std::make_exception_ptr(StorageIsDestructingException()));
      return future;
    }
    bool successful = false;
    try {
      f2(f1());
      successful = true;
    } catch (std::exception&) {
      journal_.Rollback();
    }
    if (successful) {
      persister_.PersistJournal(journal_);
      promise.set_value(TransactionResult<void>::Commited(OptionalResultExists()));
    } else {
      promise.set_value(TransactionResult<void>::Rollbacked(OptionalResultMissing()));
    }
    return future;
  }

  void GracefulShutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    destructing_ = true;
  }

 private:
  PERSISTER& persister_;
  MutationJournal& journal_;
  std::mutex mutex_;
  bool destructing_ = false;
};

}  // namespace transaction_policy
}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_TRANSACTION_POLICY_H
