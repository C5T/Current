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

namespace impl {
struct JustTransactionAsSherlockRecordType {};
}  // namespace impl

template <typename TYPELIST, template <typename> class PERSISTER, typename STREAM_RECORD_TYPE>
class SherlockStreamPersisterImpl;

template <template <typename> class PERSISTER, typename STREAM_RECORD_TYPE, typename... TS>
class SherlockStreamPersisterImpl<TypeList<TS...>, PERSISTER, STREAM_RECORD_TYPE> {
 public:
  using T_VARIANT = Variant<TS...>;
  using T_TRANSACTION = Transaction<T_VARIANT>;
  using T_SHERLOCK_ENTRY = typename std::conditional<
      std::is_same<STREAM_RECORD_TYPE, impl::JustTransactionAsSherlockRecordType>::value,
      T_TRANSACTION,
      STREAM_RECORD_TYPE>::type;

  template <typename... ARGS>
  explicit SherlockStreamPersisterImpl(ARGS&&... args)
      : stream_owned_if_any_(
            std::make_unique<sherlock::Stream<T_SHERLOCK_ENTRY, PERSISTER>>(std::forward<ARGS>(args)...)),
        stream_used_(*stream_owned_if_any_.get()) {}

  void PersistJournal(MutationJournal& journal) {
    if (!journal.commit_log.empty()) {
      T_TRANSACTION transaction;
      for (auto&& entry : journal.commit_log) {
        transaction.mutations.emplace_back(std::move(entry));
      }
      transaction.meta.timestamp = current::time::Now();
      std::swap(transaction.meta.fields, journal.meta_fields);
      stream_used_.Publish(std::move(transaction));
      journal.commit_log.clear();
      journal.rollback_log.clear();
    }
  }

  template <typename F>
  void Replay(F&& f) {
    // TODO(dkorolev) + TODO(mzhurovich): Perhaps `Replay()` should happen automatically,
    // during construction, in a blocking way?
    for (const auto& stream_record : stream_used_.InternalExposePersister().Iterate()) {
      if (Exists<T_TRANSACTION>(stream_record.entry)) {
        const T_TRANSACTION& transaction = Value<T_TRANSACTION>(stream_record.entry);
        for (const auto& mutation : transaction.mutations) {
          f(mutation);
        }
      }
    }
  }

  void ReplayTransaction(T_TRANSACTION&& transaction, current::ss::IndexAndTimestamp idx_ts) {
    if (stream_used_.Publish(transaction, idx_ts.us).index != idx_ts.index) {
      CURRENT_THROW(current::Exception());  // TODO(dkorolev): Proper exception text.
    }
  }

  void ExposeRawLogViaHTTP(int port, const std::string& route) {
    handlers_scope_ += HTTP(port).Register(route, URLPathArgs::CountMask::None, stream_used_);
  }

  sherlock::Stream<T_SHERLOCK_ENTRY, PERSISTER>& InternalExposeStream() { return stream_used_; }

  // `stream_{used/owned}_` are two variables to support both owning and non-owning Storage usage patterns.
  std::unique_ptr<sherlock::Stream<T_TRANSACTION, PERSISTER>> stream_owned_if_any_;
  sherlock::Stream<T_SHERLOCK_ENTRY, PERSISTER>& stream_used_;

  HTTPRoutesScope handlers_scope_;
};

template <typename TYPELIST, typename STREAM_RECORD_TYPE = impl::JustTransactionAsSherlockRecordType>
using SherlockInMemoryStreamPersister =
    SherlockStreamPersisterImpl<TYPELIST, current::persistence::Memory, STREAM_RECORD_TYPE>;

template <typename TYPELIST, typename STREAM_RECORD_TYPE = impl::JustTransactionAsSherlockRecordType>
using SherlockStreamPersister =
    SherlockStreamPersisterImpl<TYPELIST, current::persistence::File, STREAM_RECORD_TYPE>;

}  // namespace persister
}  // namespace storage
}  // namespace current

using current::storage::persister::SherlockInMemoryStreamPersister;
using current::storage::persister::SherlockStreamPersister;

#endif  // CURRENT_STORAGE_PERSISTER_SHERLOCK_H
