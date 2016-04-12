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

#include "common.h"
#include "../base.h"
#include "../exceptions.h"
#include "../transaction.h"
#include "../../Sherlock/sherlock.h"

#include "../../Bricks/util/locks.h"

namespace current {
namespace storage {
namespace persister {

template <typename TYPELIST, template <typename> class UNDERLYING_PERSISTER, typename STREAM_RECORD_TYPE>
class SherlockStreamPersisterImpl;

template <template <typename> class UNDERLYING_PERSISTER, typename STREAM_RECORD_TYPE, typename... TS>
class SherlockStreamPersisterImpl<TypeList<TS...>, UNDERLYING_PERSISTER, STREAM_RECORD_TYPE> {
 public:
  using variant_t = Variant<TS...>;
  using transaction_t = Transaction<variant_t>;
  using sherlock_entry_t =
      typename std::conditional<std::is_same<STREAM_RECORD_TYPE, NoCustomPersisterParam>::value,
                                transaction_t,
                                STREAM_RECORD_TYPE>::type;
  using sherlock_t = sherlock::Stream<sherlock_entry_t, UNDERLYING_PERSISTER>;
  using fields_update_function_t = std::function<void(const variant_t&)>;

  using DEPRECATED_T_(VARIANT) = variant_t;
  using DEPRECATED_T_(TRANSACTION) = transaction_t;

  struct SherlockSubscriberImpl {
    using EntryResponse = current::ss::EntryResponse;
    using TerminationResponse = current::ss::TerminationResponse;
    using replay_function_t = std::function<void(const transaction_t&)>;
    replay_function_t replay_f_;
    idxts_t& last_idxts_;

    SherlockSubscriberImpl(replay_function_t f, idxts_t& last_idxts) : replay_f_(f), last_idxts_(last_idxts) {}

    EntryResponse operator()(const transaction_t& transaction, idxts_t current, idxts_t) const {
      replay_f_(transaction);
      last_idxts_ = current;
      return EntryResponse::More;
    }

    EntryResponse EntryResponseIfNoMorePassTypeFilter() const { return EntryResponse::More; }

    TerminationResponse Terminate() { return TerminationResponse::Terminate; }
  };
  using SherlockSubscriber = current::ss::StreamSubscriber<SherlockSubscriberImpl, transaction_t>;
  using subscriber_scope_t =
      typename sherlock_t::template SyncSubscriberScope<SherlockSubscriber, transaction_t>;

  template <typename... ARGS>
  explicit SherlockStreamPersisterImpl(std::mutex& storage_mutex, fields_update_function_t f, ARGS&&... args)
      : storage_mutex_(storage_mutex),
        fields_update_f_(f),
        stream_owned_if_any_(std::make_unique<sherlock::Stream<sherlock_entry_t, UNDERLYING_PERSISTER>>(
            std::forward<ARGS>(args)...)),
        stream_used_(*stream_owned_if_any_.get()),
        authority_(PersisterDataAuthority::Own) {
    // Do not use lock since we are in ctor.
    ReplayStream<current::locks::MutexLockStatus::AlreadyLocked>();
  }

  // TODO(dkorolev): `ScopeOwnedBySomeoneElse<>` ?
  explicit SherlockStreamPersisterImpl(std::mutex& storage_mutex,
                                       fields_update_function_t f,
                                       sherlock_t& stream_owned_by_someone_else)
      : storage_mutex_(storage_mutex), fields_update_f_(f), stream_used_(stream_owned_by_someone_else) {
    authority_ = (stream_used_.DataAuthority() == current::sherlock::StreamDataAuthority::Own)
                     ? PersisterDataAuthority::Own
                     : PersisterDataAuthority::External;
    subscriber_ = std::make_unique<SherlockSubscriber>(
        [this](const transaction_t& transaction) { ApplyMutations(transaction); }, last_idxts_);
    if (authority_ == PersisterDataAuthority::Own) {
      // Do not use lock since we are in ctor.
      ReplayStream<current::locks::MutexLockStatus::AlreadyLocked>();
    } else {
      SubscribeToStream();
    }
  }

  ~SherlockStreamPersisterImpl() { TerminateStreamSubscription(); }

  PersisterDataAuthority DataAuthority() const {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    return authority_;
  }

  void PersistJournal(MutationJournal& journal) {
    if (!journal.commit_log.empty()) {
      transaction_t transaction;
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

  void ReplayTransaction(transaction_t&& transaction, current::ss::IndexAndTimestamp idx_ts) {
    if (stream_used_.Publish(transaction, idx_ts.us).index != idx_ts.index) {
      CURRENT_THROW(current::Exception());  // TODO(dkorolev): Proper exception text.
    }
  }

  void ExposeRawLogViaHTTP(int port, const std::string& route) {
    handlers_scope_ += HTTP(port).Register(route, URLPathArgs::CountMask::None, stream_used_);
  }

  sherlock_t& InternalExposeStream() { return stream_used_; }

  // This method should be called by Storage from the locked section.
  void AcquireDataAuthority() {
    if (stream_used_.DataAuthority() == current::sherlock::StreamDataAuthority::Own) {
      TerminateStreamSubscription();
      ReplayStream<current::locks::MutexLockStatus::AlreadyLocked>(subscriber_->last_idxts_.index + 1u);
      subscriber_.reset();
    } else {
      CURRENT_THROW(UnderlyingStreamHasExternalDataAuthorityException());
    }
  }

 private:
  template <current::locks::MutexLockStatus MLS>
  void ReplayStream(uint64_t from_idx = 0u) {
    for (const auto& stream_record : stream_used_.InternalExposePersister().Iterate(from_idx)) {
      if (Exists<transaction_t>(stream_record.entry)) {
        const transaction_t& transaction = Value<transaction_t>(stream_record.entry);
        ApplyMutations<MLS>(transaction);
      }
    }
  }

  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock>
  void ApplyMutations(const transaction_t& transaction) {
    current::locks::SmartMutexLockGuard<MLS> lock(storage_mutex_);
    for (const auto& mutation : transaction.mutations) {
      fields_update_f_(mutation);
    }
  }

  void SubscribeToStream() {
    assert(!subscriber_scope_);
    assert(subscriber_);
    subscriber_scope_ = std::make_unique<subscriber_scope_t>(
        std::move(stream_used_.template Subscribe<transaction_t>(*subscriber_)));
  }

  void TerminateStreamSubscription() {
    if (subscriber_scope_) {
      subscriber_scope_->Join();
      subscriber_scope_.release();
    }
  }

 private:
  std::mutex& storage_mutex_;
  fields_update_function_t fields_update_f_;
  // `stream_{used/owned}_` are two variables to support both owning and non-owning Storage usage patterns.
  std::unique_ptr<sherlock::Stream<transaction_t, UNDERLYING_PERSISTER>> stream_owned_if_any_;
  sherlock_t& stream_used_;
  idxts_t last_idxts_;
  std::unique_ptr<SherlockSubscriber> subscriber_;
  std::unique_ptr<subscriber_scope_t> subscriber_scope_;
  PersisterDataAuthority authority_;
  HTTPRoutesScope handlers_scope_;
};

template <typename TYPELIST, typename STREAM_RECORD_TYPE = NoCustomPersisterParam>
using SherlockInMemoryStreamPersister =
    SherlockStreamPersisterImpl<TYPELIST, current::persistence::Memory, STREAM_RECORD_TYPE>;

template <typename TYPELIST, typename STREAM_RECORD_TYPE = NoCustomPersisterParam>
using SherlockStreamPersister =
    SherlockStreamPersisterImpl<TYPELIST, current::persistence::File, STREAM_RECORD_TYPE>;

}  // namespace persister
}  // namespace storage
}  // namespace current

using current::storage::persister::SherlockInMemoryStreamPersister;
using current::storage::persister::SherlockStreamPersister;

#endif  // CURRENT_STORAGE_PERSISTER_SHERLOCK_H
