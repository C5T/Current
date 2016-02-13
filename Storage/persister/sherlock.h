/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef CURRENT_STORAGE_PERSISTER_SHERLOCK_H
#define CURRENT_STORAGE_PERSISTER_SHERLOCK_H

#include "../base.h"
#include "../exceptions.h"
#include "../transaction.h"
#include "../../Sherlock/sherlock.h"

namespace current {
namespace storage {
namespace persister {

template <typename TYPELIST, template <typename, typename> class PERSISTER, typename CLONER>
class SherlockStreamPersisterImpl;

template <template <typename, typename> class PERSISTER, typename CLONER, typename... TS>
class SherlockStreamPersisterImpl<TypeList<TS...>, PERSISTER, CLONER> {
 public:
  using T_VARIANT = Variant<TS...>;
  using T_TRANSACTION = Transaction<T_VARIANT>;

  template <typename... ARGS>
  explicit SherlockStreamPersisterImpl(ARGS&&... args)
      : stream_(sherlock::Stream<T_TRANSACTION, PERSISTER, CLONER>(std::forward<ARGS>(args)...)) {}

  void PersistJournal(MutationJournal& journal) {
    T_TRANSACTION transaction;
    for (auto&& entry : journal.commit_log) {
      transaction.mutations.emplace_back(std::move(entry));
    }
    transaction.meta.timestamp = current::time::Now();
    transaction.meta.fields = std::move(journal.meta_fields);
    stream_.Publish(std::move(transaction));
    journal.commit_log.clear();
    journal.rollback_log.clear();
  }

  template <typename F>
  void Replay(F&& f) {
    // TODO(dkorolev) + TODO(mzhurovich): Perhaps `Replay()` should happen automatically,
    // during construction, in a blocking way?
    SherlockProcessor processor(std::forward<F>(f));
    stream_.SyncSubscribe(processor).Join();
  }

  void ReplayTransaction(T_TRANSACTION&& transaction, blocks::ss::IndexAndTimestamp idx_ts) {
    stream_.PublishReplayed(transaction, idx_ts);
  }

  void ExposeRawLogViaHTTP(int port, const std::string& route) {
    handlers_scope_ += HTTP(port).Register(route, URLPathArgs::CountMask::None, stream_);
  }

 private:
  class SherlockProcessor {
   public:
    using IDX_TS = blocks::ss::IndexAndTimestamp;
    template <typename F>
    SherlockProcessor(F&& f)
        : f_(std::forward<F>(f)) {}

    bool operator()(T_TRANSACTION&& transaction, IDX_TS current, IDX_TS last) const {
      for (auto&& mutation : transaction.mutations) {
        f_(std::forward<T_VARIANT>(mutation));
      }
      return current.index != last.index;
    }

    void ReplayDone() { allow_terminate_ = true; }

    bool Terminate() { return allow_terminate_; }

   private:
    const std::function<void(T_VARIANT&&)> f_;
    bool allow_terminate_ = false;
  };

  sherlock::StreamInstance<T_TRANSACTION, PERSISTER, CLONER> stream_;
  HTTPRoutesScope handlers_scope_;
};

template <typename TYPELIST>
using SherlockInMemoryStreamPersister =
    SherlockStreamPersisterImpl<TYPELIST, blocks::persistence::MemoryOnly, current::DefaultCloner>;

template <typename TYPELIST>
using SherlockStreamPersister =
    SherlockStreamPersisterImpl<TYPELIST, blocks::persistence::NewAppendToFile, current::DefaultCloner>;

}  // namespace persister
}  // namespace storage
}  // namespace current

using current::storage::persister::SherlockInMemoryStreamPersister;
using current::storage::persister::SherlockStreamPersister;

#endif  // CURRENT_STORAGE_PERSISTER_SHERLOCK_H
