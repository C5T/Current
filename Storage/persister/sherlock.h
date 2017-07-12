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

template <typename MUTATIONS_VARIANT, template <typename> class UNDERLYING_PERSISTER, typename STREAM_RECORD_TYPE>
class SherlockStreamPersisterImpl final {
 public:
  using variant_t = MUTATIONS_VARIANT;
  using transaction_t = Transaction<variant_t>;
  using sherlock_entry_t = typename std::conditional<std::is_same<STREAM_RECORD_TYPE, NoCustomPersisterParam>::value,
                                                     transaction_t,
                                                     STREAM_RECORD_TYPE>::type;
  using stream_t = sherlock::Stream<sherlock_entry_t, UNDERLYING_PERSISTER>;
  using fields_update_function_t = std::function<void(const variant_t&)>;

  struct SherlockSubscriberImpl {
    using EntryResponse = current::ss::EntryResponse;
    using TerminationResponse = current::ss::TerminationResponse;
    using replay_function_t = std::function<void(const transaction_t&, std::chrono::microseconds)>;
    replay_function_t replay_f_;
    uint64_t next_replay_index_ = 0u;

    SherlockSubscriberImpl(replay_function_t f) : replay_f_(f) {}

    EntryResponse operator()(const transaction_t& transaction, idxts_t current, idxts_t) {
      replay_f_(transaction, current.us);
      next_replay_index_ = current.index + 1u;
      return EntryResponse::More;
    }

    EntryResponse operator()(std::chrono::microseconds) const { return EntryResponse::More; }

    EntryResponse EntryResponseIfNoMorePassTypeFilter() const { return EntryResponse::More; }
    TerminationResponse Terminate() const { return TerminationResponse::Terminate; }
  };
  using SherlockSubscriber = current::ss::StreamSubscriber<SherlockSubscriberImpl, transaction_t>;

  struct Master {};
  struct Following {};

  SherlockStreamPersisterImpl(Master, fields_update_function_t f, Borrowed<stream_t> stream)
      : fields_update_f_(f),
        stream_publishing_mutex_ref_(stream->Impl()->publishing_mutex),
        stream_(std::move(stream)),
        publisher_used_(stream_->BecomeFollowingStream()) {
    subscriber_instance_ = std::make_unique<SherlockSubscriber>(
        [this](const transaction_t& transaction, std::chrono::microseconds timestamp) {
          std::lock_guard<std::mutex> lock(stream_publishing_mutex_ref_);
          ApplyMutationsFromLockedSectionOrConstructor(transaction, timestamp);
        });
    std::lock_guard<std::mutex> lock(stream_publishing_mutex_ref_);
    SyncReplayStreamFromLockedSectionOrConstructor(0u);
  }

  SherlockStreamPersisterImpl(Following, fields_update_function_t f, Borrowed<stream_t> stream)
      : fields_update_f_(f),
        stream_publishing_mutex_ref_(stream->Impl()->publishing_mutex),
        stream_(std::move(stream)) {
    subscriber_instance_ = std::make_unique<SherlockSubscriber>(
        [this](const transaction_t& transaction, std::chrono::microseconds timestamp) {
          std::lock_guard<std::mutex> lock(stream_publishing_mutex_ref_);
          ApplyMutationsFromLockedSectionOrConstructor(transaction, timestamp);
        });
    std::lock_guard<std::mutex> lock(stream_publishing_mutex_ref_);
    SubscribeToStreamFromLockedSection();
  }

  ~SherlockStreamPersisterImpl() {
    std::lock_guard<std::mutex> master_follower_change_lock(master_follower_change_mutex_);
    TerminateStreamSubscriptionFromLockedSection();
  }

  template <current::locks::MutexLockStatus MLS>
  bool IsMasterStoragePersister() const {
    locks::SmartMutexLockGuard<MLS> master_follower_change_lock(master_follower_change_mutex_);
    return Exists(publisher_used_);
  }

  template <current::locks::MutexLockStatus MLS>
  std::chrono::microseconds LastAppliedTimestampPersister() const {
    locks::SmartMutexLockGuard<MLS> master_follower_change_lock(master_follower_change_mutex_);
    return last_applied_timestamp_;
  }

  void PersistJournalFromLockedSection(MutationJournal& journal) {
    const std::chrono::microseconds timestamp = current::time::Now();
    CURRENT_ASSERT(Exists(publisher_used_));
    if (!journal.commit_log.empty()) {
#ifndef CURRENT_MOCK_TIME
      CURRENT_ASSERT(journal.transaction_meta.begin_us < journal.transaction_meta.end_us);
#else
      CURRENT_ASSERT(journal.transaction_meta.begin_us <= journal.transaction_meta.end_us);
#endif
      transaction_t transaction;
      for (auto&& entry : journal.commit_log) {
        transaction.mutations.emplace_back(BypassVariantTypeCheck(), std::move(entry));
      }
      std::swap(transaction.meta, journal.transaction_meta);
      Value(publisher_used_)
          ->template Publish<current::locks::MutexLockStatus::AlreadyLocked>(std::move(transaction), timestamp);
      SetLastAppliedTimestampFromLockedSection(timestamp);
    }
    journal.Clear();
  }

  void ExposeRawLogViaHTTP(uint16_t port, const std::string& route) {
    handlers_scope_ += HTTP(port).Register(route,
                                           URLPathArgs::CountMask::None | URLPathArgs::CountMask::One,
                                           [this](Request r) { (*Borrowed<stream_t>(stream_))(std::move(r)); });
  }

  Borrowed<stream_t> BorrowStream() const { return stream_; }
  const WeakBorrowed<stream_t>& Stream() const { return stream_; }
  WeakBorrowed<stream_t>& Stream() { return stream_; }

  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock>
  Borrowed<typename stream_t::publisher_t> PublisherUsed() {
    locks::SmartMutexLockGuard<MLS> lock(master_follower_change_mutex_);
    CURRENT_ASSERT(Exists(publisher_used_));
    return Value(publisher_used_);
  }

  // Note: `BecomeMasterStorage` can not be called from a locked section, as terminating the subscriber
  // is by itself an operation that locks the publishing mutex of the stream -- in the destructor.
  void BecomeMasterStorage() {
    std::lock_guard<std::mutex> master_follower_change_lock(master_follower_change_mutex_);
    if (Exists(publisher_used_)) {
      CURRENT_THROW(StorageIsAlreadyMasterException());
    } else {
      TerminateStreamSubscriptionFromLockedSection();
      std::lock_guard<std::mutex> lock(stream_publishing_mutex_ref_);
      publisher_used_ = nullptr;
      publisher_used_ = stream_->template BecomeFollowingStream<current::locks::MutexLockStatus::AlreadyLocked>();
      const uint64_t save_replay_index = subscriber_instance_->next_replay_index_;
      subscriber_instance_ = nullptr;
      SyncReplayStreamFromLockedSectionOrConstructor(save_replay_index);
    }
  }

  // TODO(dkorolev): `BecomeFollowingStorage` maybe?

 private:
  // Invariant: both `subscriber_creator_destructor_mutex_` and `stream_publishing_mutex_ref_` are locked,
  // or the call is taking place from the constructor.
  void SyncReplayStreamFromLockedSectionOrConstructor(uint64_t from_idx) {
    for (const auto& stream_record :
         stream_->Data()->template Iterate<current::locks::MutexLockStatus::AlreadyLocked>(from_idx)) {
      if (Exists<transaction_t>(stream_record.entry)) {
        const transaction_t& transaction = Value<transaction_t>(stream_record.entry);
        ApplyMutationsFromLockedSectionOrConstructor(transaction, stream_record.idx_ts.us);
      }
    }
  }

  void ApplyMutationsFromLockedSectionOrConstructor(const transaction_t& transaction,
                                                    std::chrono::microseconds timestamp) {
    for (const auto& mutation : transaction.mutations) {
      fields_update_f_(mutation);
    }
    SetLastAppliedTimestampFromLockedSection(timestamp);
  }

 private:
  // Invariant: `master_follower_change_mutex_` is locked, or the call is happening from the constructor.
  void SubscribeToStreamFromLockedSection() {
    CURRENT_ASSERT(!subscriber_scope_);
    CURRENT_ASSERT(subscriber_instance_);
    subscriber_scope_ = std::move(stream_->template Subscribe<transaction_t>(*subscriber_instance_));
  }

  // Invariant: `master_follower_change_mutex_` is locked.
  // Important: The publishing mutex of the respective stream must be unlocked!
  void TerminateStreamSubscriptionFromLockedSection() { subscriber_scope_ = nullptr; }

  void SetLastAppliedTimestampFromLockedSection(std::chrono::microseconds timestamp) {
    CURRENT_ASSERT(timestamp > last_applied_timestamp_);
    last_applied_timestamp_ = timestamp;
  }

 private:
  fields_update_function_t fields_update_f_;

  std::mutex& stream_publishing_mutex_ref_;  // == `stream_->Impl()->publishing_mutex`.
  Borrowed<stream_t> stream_;
  Optional<Borrowed<typename stream_t::publisher_t>> publisher_used_;  // Set iff the storage is the master storage.

  mutable std::mutex master_follower_change_mutex_;
  std::unique_ptr<SherlockSubscriber> subscriber_instance_;
  current::sherlock::SubscriberScope subscriber_scope_;

  std::chrono::microseconds last_applied_timestamp_ = std::chrono::microseconds(-1);  // Replayed or from the master.

  HTTPRoutesScope handlers_scope_;
};

template <typename TYPELIST, typename STREAM_RECORD_TYPE = NoCustomPersisterParam>
using SherlockInMemoryStreamPersister =
    SherlockStreamPersisterImpl<TYPELIST, current::persistence::Memory, STREAM_RECORD_TYPE>;

template <typename TYPELIST, typename STREAM_RECORD_TYPE = NoCustomPersisterParam>
using SherlockStreamPersister = SherlockStreamPersisterImpl<TYPELIST, current::persistence::File, STREAM_RECORD_TYPE>;

}  // namespace persister
}  // namespace storage
}  // namespace current

using current::storage::persister::SherlockInMemoryStreamPersister;
using current::storage::persister::SherlockStreamPersister;

#endif  // CURRENT_STORAGE_PERSISTER_SHERLOCK_H
