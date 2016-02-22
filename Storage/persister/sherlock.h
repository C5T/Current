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

template <typename TYPELIST, template <typename> class PERSISTER>
class SherlockStreamPersisterImpl;

template <template <typename> class PERSISTER, typename... TS>
class SherlockStreamPersisterImpl<TypeList<TS...>, PERSISTER> {
 public:
  using T_VARIANT = Variant<TS...>;
  using T_TRANSACTION = Transaction<T_VARIANT>;

  template <typename... ARGS>
  explicit SherlockStreamPersisterImpl(ARGS&&... args)
      : stream_(sherlock::Stream<T_TRANSACTION, PERSISTER>(std::forward<ARGS>(args)...)) {}

  void PersistJournal(MutationJournal& journal) {
    if (!journal.commit_log.empty()) {
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
  }

  template <typename F>
  void Replay(F&& f) {
    // TODO(dkorolev) + TODO(mzhurovich): Perhaps `Replay()` should happen automatically,
    // during construction, in a blocking way?
    for (const auto& transaction : stream_.InternalExposePersister().Iterate()) {
      for (const auto& mutation : transaction.entry.mutations) {
        f(mutation);
      }
    }
  }

  void ReplayTransaction(T_TRANSACTION&& transaction, current::ss::IndexAndTimestamp idx_ts) {
    stream_.PublishReplayed(transaction, idx_ts);
  }

  void ExposeRawLogViaHTTP(int port, const std::string& route) {
    handlers_scope_ += HTTP(port).Register(route, URLPathArgs::CountMask::None, stream_);
  }

  sherlock::Stream<T_TRANSACTION, PERSISTER>& InternalExposeStream() { return stream_; }

  sherlock::Stream<T_TRANSACTION, PERSISTER> stream_;
  HTTPRoutesScope handlers_scope_;
};

template <typename TYPELIST>
using SherlockInMemoryStreamPersister = SherlockStreamPersisterImpl<TYPELIST, current::persistence::Memory>;

template <typename TYPELIST>
using SherlockStreamPersister = SherlockStreamPersisterImpl<TYPELIST, current::persistence::File>;

}  // namespace persister
}  // namespace storage
}  // namespace current

using current::storage::persister::SherlockInMemoryStreamPersister;
using current::storage::persister::SherlockStreamPersister;

#endif  // CURRENT_STORAGE_PERSISTER_SHERLOCK_H
