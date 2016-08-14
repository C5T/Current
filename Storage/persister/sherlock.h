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

#include "../../Bricks/sync/locks.h"

namespace current {
namespace storage {
namespace persister {

template <typename MUTATIONS_VARIANT,
          template <typename> class UNDERLYING_PERSISTER,
          typename STREAM_RECORD_TYPE>
class SherlockStreamPersisterImpl {
 public:
  using variant_t = MUTATIONS_VARIANT;
  using transaction_t = Transaction<variant_t>;
  using sherlock_entry_t =
      typename std::conditional<std::is_same<STREAM_RECORD_TYPE, NoCustomPersisterParam>::value,
                                transaction_t,
                                STREAM_RECORD_TYPE>::type;
  using sherlock_t = sherlock::Stream<sherlock_entry_t, UNDERLYING_PERSISTER>;
  using fields_update_function_t = std::function<void(const variant_t&)>;

  struct SherlockSubscriberImpl {
    using EntryResponse = current::ss::EntryResponse;
    using TerminationResponse = current::ss::TerminationResponse;
    using replay_function_t = std::function<void(const transaction_t&)>;
    replay_function_t replay_f_;
    uint64_t next_replay_index_ = 0u;

    SherlockSubscriberImpl(replay_function_t f) : replay_f_(f) {}

    EntryResponse operator()(const transaction_t& transaction, idxts_t current, idxts_t) {
      replay_f_(transaction);
      next_replay_index_ = current.index + 1u;
      return EntryResponse::More;
    }

    EntryResponse EntryResponseIfNoMorePassTypeFilter() const { return EntryResponse::More; }
    TerminationResponse Terminate() const { return TerminationResponse::Terminate; }
  };
  using SherlockSubscriber = current::ss::StreamSubscriber<SherlockSubscriberImpl, transaction_t>;

  template <typename... ARGS>
  explicit SherlockStreamPersisterImpl(std::mutex& storage_mutex, fields_update_function_t f, ARGS&&... args)
      : storage_mutex_ref_(storage_mutex),
        fields_update_f_(f),
        stream_owned_if_any_(std::make_unique<sherlock::Stream<sherlock_entry_t, UNDERLYING_PERSISTER>>(
            std::forward<ARGS>(args)...)),
        stream_used_(*stream_owned_if_any_.get()),
        authority_(PersisterDataAuthority::Own) {
    // Do not use lock since we are in ctor.
    SyncReplayStream<current::locks::MutexLockStatus::AlreadyLocked>();
  }

  // TODO(dkorolev): `ScopeOwnedBySomeoneElse<>` ?
  explicit SherlockStreamPersisterImpl(std::mutex& storage_mutex,
                                       fields_update_function_t f,
                                       sherlock_t& stream_owned_by_someone_else)
      : storage_mutex_ref_(storage_mutex), fields_update_f_(f), stream_used_(stream_owned_by_someone_else) {
    authority_ = (stream_used_.DataAuthority() == current::sherlock::StreamDataAuthority::Own)
                     ? PersisterDataAuthority::Own
                     : PersisterDataAuthority::External;
    subscriber_ = std::make_unique<SherlockSubscriber>(
        [this](const transaction_t& transaction) { ApplyMutations(transaction); });
    if (authority_ == PersisterDataAuthority::Own) {
      // Do not use lock since we are in ctor.
      SyncReplayStream<current::locks::MutexLockStatus::AlreadyLocked>();
    } else {
      SubscribeToStream();
    }
  }

  ~SherlockStreamPersisterImpl() { TerminateStreamSubscription(); }

  PersisterDataAuthority DataAuthority() const {
    std::lock_guard<std::mutex> lock(storage_mutex_ref_);
    return authority_;
  }

  void PersistJournal(MutationJournal& journal) {
    if (!journal.commit_log.empty()) {
#ifndef CURRENT_MOCK_TIME
      assert(journal.transaction_meta.begin_us < journal.transaction_meta.end_us);
#else
      assert(journal.transaction_meta.begin_us <= journal.transaction_meta.end_us);
#endif
      transaction_t transaction;
      for (auto&& entry : journal.commit_log) {
        transaction.mutations.emplace_back(BypassVariantTypeCheck(), std::move(entry));
      }
      std::swap(transaction.meta, journal.transaction_meta);
      stream_used_.Publish(std::move(transaction));
    }
    journal.Clear();
  }

  void ExposeRawLogViaHTTP(uint16_t port, const std::string& route) {
    handlers_scope_ +=
        HTTP(port).Register(route, URLPathArgs::CountMask::None | URLPathArgs::CountMask::One, stream_used_);
  }

  sherlock_t& InternalExposeStream() { return stream_used_; }

  template <current::locks::MutexLockStatus MLS>
  void AcquireDataAuthority() {
    current::locks::SmartMutexLockGuard<MLS> lock(storage_mutex_ref_);
    if (stream_used_.DataAuthority() == current::sherlock::StreamDataAuthority::Own) {
      TerminateStreamSubscription();
      SyncReplayStream<current::locks::MutexLockStatus::AlreadyLocked>(subscriber_->next_replay_index_);
      subscriber_ = nullptr;
    } else {
      CURRENT_THROW(UnderlyingStreamHasExternalDataAuthorityException());
    }
  }

 private:
  template <current::locks::MutexLockStatus MLS>
  void SyncReplayStream(uint64_t from_idx = 0u) {
    for (const auto& stream_record : stream_used_.InternalExposePersister().Iterate(from_idx)) {
      if (Exists<transaction_t>(stream_record.entry)) {
        const transaction_t& transaction = Value<transaction_t>(stream_record.entry);
        ApplyMutations<MLS>(transaction);
      }
    }
  }

  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock>
  void ApplyMutations(const transaction_t& transaction) {
    current::locks::SmartMutexLockGuard<MLS> lock(storage_mutex_ref_);
    for (const auto& mutation : transaction.mutations) {
      fields_update_f_(mutation);
    }
  }

  void SubscribeToStream() {
    assert(!subscriber_scope_);
    assert(subscriber_);
    subscriber_scope_ = std::move(stream_used_.template Subscribe<transaction_t>(*subscriber_));
  }

  void TerminateStreamSubscription() { subscriber_scope_ = nullptr; }

 private:
  std::mutex& storage_mutex_ref_;
  fields_update_function_t fields_update_f_;
  // `stream_{used/owned}_` are two variables to support both owning and non-owning Storage usage patterns.
  std::unique_ptr<sherlock::Stream<transaction_t, UNDERLYING_PERSISTER>> stream_owned_if_any_;
  sherlock_t& stream_used_;
  std::unique_ptr<SherlockSubscriber> subscriber_;
  current::sherlock::SubscriberScope subscriber_scope_;
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
