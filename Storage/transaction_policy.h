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

#include "../Blocks/SS/ss.h"
#include "../Blocks/Persistence/exceptions.h"

namespace current {
namespace storage {
namespace transaction_policy {

template <class PERSISTER>
class Synchronous final {
 public:
  using T_TRANSACTION = typename PERSISTER::T_TRANSACTION;

  Synchronous(PERSISTER& persister, MutationJournal& journal) : persister_(persister), journal_(journal) {}

  ~Synchronous() {
    std::lock_guard<std::mutex> lock(mutex_);
    destructing_ = true;
  }

  template <typename F>
  using T_F_RESULT = typename std::result_of<F()>::type;

  template <typename F, class = ENABLE_IF<!std::is_void<T_F_RESULT<F>>::value>>
  Future<TransactionResult<T_F_RESULT<F>>, StrictFuture::Strict> Transaction(
      F&& f, TransactionMetaFields&& meta_fields) {
    using T_RESULT = T_F_RESULT<F>;
    std::lock_guard<std::mutex> lock(mutex_);
    journal_.AssertEmpty();
    std::promise<TransactionResult<T_RESULT>> promise;
    Future<TransactionResult<T_RESULT>, StrictFuture::Strict> future = promise.get_future();
    if (destructing_) {
      promise.set_exception(std::make_exception_ptr(StorageInGracefulShutdownException()));  // LCOV_EXCL_LINE
      return future;                                                                         // LCOV_EXCL_LINE
    }
    bool successful = false;
    T_RESULT f_result;
    try {
      f_result = f();
      successful = true;
    } catch (StorageRollbackExceptionWithValue<T_RESULT> e) {
      journal_.Rollback();
      promise.set_value(TransactionResult<T_RESULT>::RolledBack(std::move(e.value)));
    } catch (StorageRollbackExceptionWithNoValue) {
      journal_.Rollback();
      promise.set_value(TransactionResult<T_RESULT>::RolledBack(OptionalResultMissing()));
    } catch (...) {  // The exception is captured with `std::current_exception()` below.
      journal_.Rollback();
      // LCOV_EXCL_START
      try {
        promise.set_exception(std::current_exception());
      } catch (const std::exception& e) {
        std::cerr << "`promise.set_exception()` failed in Synchronous::Transaction: " << e.what() << std::endl;
        std::exit(-1);
      }
      // LCOV_EXCL_STOP
    }
    if (successful) {
      journal_.meta_fields = std::move(meta_fields);
      PersistJournal();
      promise.set_value(TransactionResult<T_RESULT>::Committed(std::move(f_result)));
    }
    return future;
  }

  template <typename F, class = ENABLE_IF<std::is_void<T_F_RESULT<F>>::value>>
  Future<TransactionResult<void>, StrictFuture::Strict> Transaction(F&& f,
                                                                    TransactionMetaFields&& meta_fields) {
    std::lock_guard<std::mutex> lock(mutex_);
    journal_.AssertEmpty();
    std::promise<TransactionResult<void>> promise;
    Future<TransactionResult<void>, StrictFuture::Strict> future = promise.get_future();
    if (destructing_) {
      promise.set_exception(std::make_exception_ptr(StorageInGracefulShutdownException()));
      return future;
    }
    bool successful = false;
    try {
      f();
      successful = true;
    } catch (StorageRollbackExceptionWithNoValue) {
      journal_.Rollback();
      promise.set_value(TransactionResult<void>::RolledBack(OptionalResultExists()));
    } catch (...) {  // The exception is captured with `std::current_exception()` below.
      journal_.Rollback();
      // LCOV_EXCL_START
      try {
        promise.set_exception(std::current_exception());
      } catch (const std::exception& e) {
        std::cerr << "`promise.set_exception()` failed in Synchronous::Transaction: " << e.what() << std::endl;
        std::exit(-1);
      }
      // LCOV_EXCL_STOP
    }
    if (successful) {
      journal_.meta_fields = std::move(meta_fields);
      PersistJournal();
      promise.set_value(TransactionResult<void>::Committed(OptionalResultExists()));
    }
    return future;
  }

  // TODO(mz+dk): implement proper logic here (consider rollbacks & exceptions).
  template <typename F1, typename F2, class = ENABLE_IF<!std::is_void<T_F_RESULT<F1>>::value>>
  Future<TransactionResult<void>, StrictFuture::Strict> Transaction(F1&& f1,
                                                                    F2&& f2,
                                                                    TransactionMetaFields&& meta_fields) {
    using T_RESULT = T_F_RESULT<F1>;
    std::lock_guard<std::mutex> lock(mutex_);
    journal_.AssertEmpty();
    std::promise<TransactionResult<void>> promise;
    Future<TransactionResult<void>, StrictFuture::Strict> future = promise.get_future();
    if (destructing_) {
      promise.set_exception(std::make_exception_ptr(StorageInGracefulShutdownException()));  // LCOV_EXCL_LINE
      return future;                                                                         // LCOV_EXCL_LINE
    }
    bool successful = false;
    T_RESULT f1_result;
    try {
      f1_result = f1();
      f2(std::move(f1_result));
      successful = true;
    } catch (StorageRollbackExceptionWithValue<T_RESULT> e) {
      // Transaction was rolled back, but returned a value, which we try to pass again to `f2`.
      journal_.Rollback();
      f2(std::move(e.value));
      promise.set_value(TransactionResult<void>::RolledBack(OptionalResultMissing()));
    } catch (StorageRollbackExceptionWithNoValue) {
      // Transaction was rolled back and returned nothing we can pass to `f2`.
      journal_.Rollback();
      promise.set_value(TransactionResult<void>::RolledBack(OptionalResultMissing()));
    } catch (...) {  // The exception is captured with `std::current_exception()` below.
      // LCOV_EXCL_START
      journal_.Rollback();
      try {
        promise.set_exception(std::current_exception());
      } catch (const std::exception& e) {
        std::cerr << "`promise.set_exception()` failed in Synchronous::Transaction: " << e.what() << std::endl;
        std::exit(-1);
      }
      // LCOV_EXCL_STOP
    }
    if (successful) {
      journal_.meta_fields = std::move(meta_fields);
      PersistJournal();
      promise.set_value(TransactionResult<void>::Committed(OptionalResultExists()));
    }
    return future;
  }

  template <typename F>
  void ReplayTransaction(F&& f, T_TRANSACTION&& transaction, current::ss::IndexAndTimestamp idx_ts) {
    std::lock_guard<std::mutex> lock(mutex_);
    persister_.ReplayTransaction(std::forward<T_TRANSACTION>(transaction), idx_ts);
    for (auto&& mutation : transaction.mutations) {
      f(std::move(mutation));
    }
  }

  void GracefulShutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    destructing_ = true;
  }

 private:
  void PersistJournal() {
    try {
      persister_.PersistJournal(journal_);
    } catch (const persistence::InconsistentTimestampException& e) {
      std::cerr << "PersistJournal() failed with InconsistentTimestampException: " << e.what() << std::endl;
#ifdef CURRENT_MOCK_TIME
      std::cerr << "Binary is compiled with `CURRENT_MOCK_TIME`. Probably `SetNow()` wasn't properly called."
                << std::endl;
#endif
      std::exit(-1);
    } catch (const std::exception& e) {
      std::cerr << "PersistJournal() failed with exception: " << e.what() << std::endl;
      std::exit(-1);
    }
  }

  PERSISTER& persister_;
  MutationJournal& journal_;
  std::mutex mutex_;
  bool destructing_ = false;
};

}  // namespace transaction_policy
}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_TRANSACTION_POLICY_H
