/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#include "event_store.h"

// #include "../../3rdparty/gtest/gtest-main-with-dflags.h"
#include "../../3rdparty/gtest/gtest-main.h"

TEST(EventStore, Demo) {
  // TODO(dkorolev) + TODO(mzhurovich): Convert all `T_UPPER_CASE` into `upper_case_t` ?

  // TODO(dkorolev): This `SherlockInMemoryStreamPersister` thing is redundant here. Something to think of.
  using transaction_t = typename EventStoreDB<SherlockInMemoryStreamPersister>::T_TRANSACTION;
  using stream_type_t = Variant<transaction_t, EventOutsideStorage>;

  using event_store_storage_t = EventStoreDB<SherlockInMemoryStreamPersister,
                                             current::storage::transaction_policy::Synchronous,
                                             stream_type_t>;
  using full_stream_t = typename event_store_storage_t::T_PERSISTER::T_SHERLOCK;

  using event_store_t = EventStore<event_store_storage_t>;
  using db_t = typename event_store_t::db_t;

  using nonstorage_stream_t = current::sherlock::Stream<EventOutsideStorage, current::persistence::Memory>;

  full_stream_t full_event_log;
  nonstorage_stream_t readonly_nonstorage_event_log;
  event_store_t event_store(full_event_log);

  struct ReadonlyStreamFollower {
    using emitter_t = std::function<void(const EventOutsideStorage&, idxts_t)>;
    const emitter_t emitter;
    explicit ReadonlyStreamFollower(emitter_t emitter) : emitter(emitter) {}
    current::ss::EntryResponse operator()(const EventOutsideStorage& e, idxts_t current, idxts_t) const {
      emitter(e, current);
      return current::ss::EntryResponse::More;
    }
    static current::ss::TerminationResponse Terminate() { return current::ss::TerminationResponse::Terminate; }
    static current::ss::EntryResponse EntryResponseIfNoMorePassTypeFilter() {
      return current::ss::EntryResponse::More;
    }
  };
  const auto emitter = [&readonly_nonstorage_event_log](const EventOutsideStorage& e, idxts_t idx_ts) {
    readonly_nonstorage_event_log.Publish(e, idx_ts.us);
  };
  current::ss::StreamSubscriber<ReadonlyStreamFollower, EventOutsideStorage> readonly_stream_follower(emitter);
  auto readonly_stream_follower_scope = full_event_log.Subscribe<EventOutsideStorage>(readonly_stream_follower);

  const auto& readonly_nonstorage_event_log_persister = readonly_nonstorage_event_log.InternalExposePersister();

  EXPECT_EQ(0u, readonly_nonstorage_event_log_persister.Size());

  const auto add_event_result = event_store.db.Transaction([](MutableFields<db_t> fields) {
    EXPECT_TRUE(fields.events.Empty());
    Event event;
    event.key = "id";
    event.body.some_event_data = "foo";
    fields.events.Add(event);
  }).Go();
  EXPECT_TRUE(WasCommitted(add_event_result));

  const auto verify_event_added_result = event_store.db.Transaction([](ImmutableFields<db_t> fields) {
    EXPECT_EQ(1u, fields.events.Size());
    EXPECT_TRUE(Exists(fields.events["id"]));
    EXPECT_EQ("foo", Value(fields.events["id"]).body.some_event_data);
  }).Go();
  EXPECT_TRUE(WasCommitted(verify_event_added_result));

  EXPECT_EQ(0u, readonly_nonstorage_event_log_persister.Size());

  {
    EventOutsideStorage e;
    e.message = "haha";
    full_event_log.Publish(e);
  }

  while (readonly_nonstorage_event_log_persister.Size() < 1u) {
    ;  // Spin lock.
  }
  EXPECT_EQ(1u, readonly_nonstorage_event_log_persister.Size());
  EXPECT_EQ("haha", (*readonly_nonstorage_event_log_persister.Iterate(0u, 1u).begin()).entry.message);

  readonly_stream_follower_scope.Join();
}
