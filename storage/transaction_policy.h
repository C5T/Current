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

#include <type_traits>

#include "base.h"
#include "exceptions.h"
#include "transaction_result.h"

#include "../bricks/util/future.h"

#include "../blocks/ss/ss.h"
#include "../blocks/persistence/exceptions.h"

namespace current {
namespace storage {
namespace transaction_policy {

template <class PERSISTER>
class Synchronous final {
 public:
  using transaction_t = typename PERSISTER::transaction_t;

  Synchronous(PERSISTER& persister, MutationJournal& journal)
      : persister_(persister), journal_(journal), destructing_(false) {}

  ~Synchronous() { destructing_ = true; }

  template <typename F>
  using f_result_t = std::invoke_result_t<F>;

  // Read-write transaction returning non-void type.
  template <typename F, class = std::enable_if_t<!std::is_void<f_result_t<F>>::value>>
  Future<TransactionResult<f_result_t<F>>, StrictFuture::Strict> TransactionFromLockedSection(F&& f) {
    using result_t = f_result_t<F>;
    journal_.AssertEmpty();
    std::promise<TransactionResult<result_t>> promise;
    if (destructing_) {
      promise.set_exception(std::make_exception_ptr(StorageInGracefulShutdownException()));  // LCOV_EXCL_LINE
    } else {
      bool successful = false;
      result_t f_result;
      try {
        journal_.BeforeTransaction();
        f_result = f();
        journal_.AfterTransaction();
        successful = true;
      } catch (StorageRollbackExceptionWithValue<result_t> e) {
        journal_.Rollback();
        promise.set_value(TransactionResult<result_t>::RolledBack(std::move(e.value)));
      } catch (StorageRollbackExceptionWithNoValue) {
        journal_.Rollback();
        promise.set_value(TransactionResult<result_t>::RolledBack(OptionalResultMissing()));
      } catch (...) {  // The exception is captured with `std::current_exception()` below.
        journal_.Rollback();
        // LCOV_EXCL_START
        try {
          promise.set_exception(std::current_exception());
        } catch (const std::exception& e) {
          std::cerr << "`promise.set_exception()` failed in Synchronous::TransactionFromLockedSection: " << e.what()
                    << std::endl;
          std::exit(-1);
        }
        // LCOV_EXCL_STOP
      }
      if (successful) {
        PersistJournal();
        promise.set_value(TransactionResult<result_t>::Committed(std::move(f_result)));
      }
    }
    return Future<TransactionResult<result_t>, StrictFuture::Strict>(promise.get_future());
  }

  // Read-only transaction returning non-void type.
  template <typename F, class = std::enable_if_t<!std::is_void<f_result_t<F>>::value>>
  Future<TransactionResult<f_result_t<F>>, StrictFuture::Strict> TransactionFromLockedSection(F&& f) const {
    using result_t = f_result_t<F>;
    journal_.AssertEmpty();
    std::promise<TransactionResult<result_t>> promise;
    if (destructing_) {
      promise.set_exception(std::make_exception_ptr(StorageInGracefulShutdownException()));  // LCOV_EXCL_LINE
    } else {
      bool successful = false;
      result_t f_result;
      try {
        f_result = f();
        successful = true;
      } catch (StorageRollbackExceptionWithValue<result_t>& e) {
        promise.set_value(TransactionResult<result_t>::RolledBack(std::move(e.value)));
      } catch (const StorageRollbackExceptionWithNoValue&) {
        promise.set_value(TransactionResult<result_t>::RolledBack(OptionalResultMissing()));
      } catch (...) {  // The exception is captured with `std::current_exception()` below.
        // LCOV_EXCL_START
        try {
          promise.set_exception(std::current_exception());
        } catch (const std::exception& e) {
          std::cerr << "`promise.set_exception()` failed in Synchronous::TransactionFromLockedSection: " << e.what()
                    << std::endl;
          std::exit(-1);
        }
        // LCOV_EXCL_STOP
      }
      if (successful) {
        promise.set_value(TransactionResult<result_t>::Committed(std::move(f_result)));
      }
    }
    return Future<TransactionResult<result_t>, StrictFuture::Strict>(promise.get_future());
  }

  // Read-write transaction returning void type.
  template <typename F, class = std::enable_if_t<std::is_void<f_result_t<F>>::value>>
  Future<TransactionResult<void>, StrictFuture::Strict> TransactionFromLockedSection(F&& f) {
    journal_.AssertEmpty();
    std::promise<TransactionResult<void>> promise;
    if (destructing_) {
      // LCOV_EXCL_START
      promise.set_exception(std::make_exception_ptr(StorageInGracefulShutdownException()));
      // LCOV_EXCL_STOP
    } else {
      bool successful = false;
      try {
        journal_.BeforeTransaction();
        f();
        journal_.AfterTransaction();
        successful = true;
      } catch (const StorageRollbackExceptionWithNoValue&) {
        journal_.Rollback();
        promise.set_value(TransactionResult<void>::RolledBack(OptionalResultExists()));
      } catch (...) {  // The exception is captured with `std::current_exception()` below.
        journal_.Rollback();
        // LCOV_EXCL_START
        try {
          promise.set_exception(std::current_exception());
        } catch (const std::exception& e) {
          std::cerr << "`promise.set_exception()` failed in Synchronous::TransactionFromLockedSection: " << e.what()
                    << std::endl;
          std::exit(-1);
        }
        // LCOV_EXCL_STOP
      }
      if (successful) {
        PersistJournal();
        promise.set_value(TransactionResult<void>::Committed(OptionalResultExists()));
      }
    }
    return Future<TransactionResult<void>, StrictFuture::Strict>(promise.get_future());
  }

  // Read-only transaction returning void type.
  template <typename F, class = std::enable_if_t<std::is_void<f_result_t<F>>::value>>
  Future<TransactionResult<void>, StrictFuture::Strict> TransactionFromLockedSection(F&& f) const {
    journal_.AssertEmpty();
    std::promise<TransactionResult<void>> promise;
    if (destructing_) {
      promise.set_exception(std::make_exception_ptr(StorageInGracefulShutdownException()));
    } else {
      bool successful = false;
      try {
        f();
        successful = true;
      } catch (const StorageRollbackExceptionWithNoValue&) {
        promise.set_value(TransactionResult<void>::RolledBack(OptionalResultExists()));
      } catch (...) {  // The exception is captured with `std::current_exception()` below.
        // LCOV_EXCL_START
        try {
          promise.set_exception(std::current_exception());
        } catch (const std::exception& e) {
          std::cerr << "`promise.set_exception()` failed in Synchronous::TransactionFromLockedSection: " << e.what()
                    << std::endl;
          std::exit(-1);
        }
        // LCOV_EXCL_STOP
      }
      if (successful) {
        promise.set_value(TransactionResult<void>::Committed(OptionalResultExists()));
      }
    }
    return Future<TransactionResult<void>, StrictFuture::Strict>(promise.get_future());
  }

  // TODO(mz+dk): implement proper logic here (consider rollbacks & exceptions).
  // Read-write two-step transaction.
  template <typename F1, typename F2, class = std::enable_if_t<!std::is_void<f_result_t<F1>>::value>>
  Future<TransactionResult<void>, StrictFuture::Strict> TransactionFromLockedSection(F1&& f1, F2&& f2) {
    using result_t = f_result_t<F1>;
    journal_.AssertEmpty();
    std::promise<TransactionResult<void>> promise;
    if (destructing_) {
      promise.set_exception(std::make_exception_ptr(StorageInGracefulShutdownException()));  // LCOV_EXCL_LINE
    } else {
      result_t f1_result;
      try {
        journal_.BeforeTransaction();
        f1_result = f1();
        journal_.AfterTransaction();
        PersistJournal();
        f2(std::move(f1_result));
        promise.set_value(TransactionResult<void>::Committed(OptionalResultExists()));
      } catch (const StorageRollbackExceptionWithValue<result_t>& e) {
        // The transaction was rolled back, but returned a value, which we try to pass again to `f2`.
        journal_.Rollback();
        f2(std::move(e.value));
        promise.set_value(TransactionResult<void>::RolledBack(OptionalResultMissing()));
      } catch (const StorageRollbackExceptionWithNoValue&) {
        // The transaction was rolled back and returned nothing we can pass to `f2`.
        journal_.Rollback();
        promise.set_value(TransactionResult<void>::RolledBack(OptionalResultMissing()));
      } catch (...) {  // The exception is captured with `std::current_exception()` below.
        // LCOV_EXCL_START
        journal_.Rollback();
        try {
          promise.set_exception(std::current_exception());
        } catch (const std::exception& e) {
          std::cerr << "`promise.set_exception()` failed in Synchronous::TransactionFromLockedSection: " << e.what()
                    << std::endl;
          std::exit(-1);
        }
        // LCOV_EXCL_STOP
      }
    }
    return Future<TransactionResult<void>, StrictFuture::Strict>(promise.get_future());
  }

  // Read-only two-step transaction.
  template <typename F1, typename F2, class = std::enable_if_t<!std::is_void<f_result_t<F1>>::value>>
  Future<TransactionResult<void>, StrictFuture::Strict> TransactionFromLockedSection(F1&& f1, F2&& f2) const {
    using result_t = f_result_t<F1>;
    journal_.AssertEmpty();
    std::promise<TransactionResult<void>> promise;
    if (destructing_) {
      promise.set_exception(std::make_exception_ptr(StorageInGracefulShutdownException()));  // LCOV_EXCL_LINE
    } else {
      try {
        f2(f1());
        promise.set_value(TransactionResult<void>::Committed(OptionalResultExists()));
      } catch (StorageRollbackExceptionWithValue<result_t>& e) {
        // The transaction was rolled back, but returned a value, which we try to pass again to `f2`.
        f2(std::move(e.value));
        promise.set_value(TransactionResult<void>::RolledBack(OptionalResultMissing()));
      } catch (const StorageRollbackExceptionWithNoValue&) {
        // The transaction was rolled back and returned nothing we can pass to `f2`.
        promise.set_value(TransactionResult<void>::RolledBack(OptionalResultMissing()));
      } catch (...) {  // The exception is captured with `std::current_exception()` below.
        // LCOV_EXCL_START
        try {
          promise.set_exception(std::current_exception());
        } catch (const std::exception& e) {
          std::cerr << "`promise.set_exception()` failed in Synchronous::TransactionFromLockedSection: " << e.what()
                    << std::endl;
          std::exit(-1);
        }
        // LCOV_EXCL_STOP
      }
    }
    return Future<TransactionResult<void>, StrictFuture::Strict>(promise.get_future());
  }

  void GracefulShutdown() { destructing_ = true; }

 private:
  void PersistJournal() {
    try {
      persister_.PersistJournalFromLockedSection(journal_);
    } catch (const ss::InconsistentTimestampException& e) {
      std::cerr << "PersistJournal() failed with InconsistentTimestampException: " << e.what() << std::endl;
#ifdef CURRENT_MOCK_TIME
      std::cerr << "The binary is compiled with `CURRENT_MOCK_TIME`. Probably, `SetNow()` wasn't properly called."
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
  std::atomic_bool destructing_;
};

}  // namespace transaction_policy
}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_TRANSACTION_POLICY_H
