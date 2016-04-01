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
#include "schema.h"

#include "../../Bricks/dflags/dflags.h"
#include "../../Bricks/file/file.h"

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

#ifndef _MSC_VER
DEFINE_string(event_store_test_tmpdir, ".current", "Local path for the test to create temporary files in.");
#else
DEFINE_string(event_store_test_tmpdir,
              ".",
              "Local path for the test to create temporary files in.");
#endif

TEST(EventStore, SmokeWithInMemoryEventStore) {
  using event_store_t = EventStore<EventStoreDB, EventOutsideStorage, SherlockInMemoryStreamPersister>;
  using db_t = event_store_t::event_store_storage_t;

  event_store_t event_store;

  EXPECT_EQ(0u, event_store.readonly_nonstorage_event_log_persister.Size());

  const auto add_event_result = event_store.event_store_storage.Transaction([](MutableFields<db_t> fields) {
    EXPECT_TRUE(fields.events.Empty());
    Event event;
    event.key = "id";
    event.body.some_event_data = "foo";
    fields.events.Add(event);
  }).Go();
  EXPECT_TRUE(WasCommitted(add_event_result));

  const auto verify_event_added_result =
      event_store.event_store_storage.Transaction([](ImmutableFields<db_t> fields) {
        EXPECT_EQ(1u, fields.events.Size());
        EXPECT_TRUE(Exists(fields.events["id"]));
        EXPECT_EQ("foo", Value(fields.events["id"]).body.some_event_data);
      }).Go();
  EXPECT_TRUE(WasCommitted(verify_event_added_result));

  EXPECT_EQ(0u, event_store.readonly_nonstorage_event_log_persister.Size());

  {
    EventOutsideStorage e;
    e.message = "haha";
    event_store.full_event_log.Publish(e);
  }

  while (event_store.readonly_nonstorage_event_log_persister.Size() < 1u) {
    ;  // Spin lock.
  }
  EXPECT_EQ(1u, event_store.readonly_nonstorage_event_log_persister.Size());
  EXPECT_EQ("haha",
            (*event_store.readonly_nonstorage_event_log_persister.Iterate(0u, 1u).begin()).entry.message);
}

TEST(EventStore, SmokeWithDiskPersistedEventStore) {
  const std::string persistence_file_name =
      current::FileSystem::JoinPath(FLAGS_event_store_test_tmpdir, ".current_testdb");
  const auto persistence_file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

  using event_store_t = EventStore<EventStoreDB, EventOutsideStorage, SherlockStreamPersister>;
  using db_t = event_store_t::event_store_storage_t;

  event_store_t event_store(persistence_file_name);

  EXPECT_EQ(0u, event_store.readonly_nonstorage_event_log_persister.Size());

  const auto add_event_result = event_store.event_store_storage.Transaction([](MutableFields<db_t> fields) {
    EXPECT_TRUE(fields.events.Empty());
    Event event;
    event.key = "another_id";
    event.body.some_event_data = "bar";
    fields.events.Add(event);
  }).Go();
  EXPECT_TRUE(WasCommitted(add_event_result));

  const auto verify_event_added_result =
      event_store.event_store_storage.Transaction([](ImmutableFields<db_t> fields) {
        EXPECT_EQ(1u, fields.events.Size());
        EXPECT_TRUE(Exists(fields.events["another_id"]));
        EXPECT_EQ("bar", Value(fields.events["another_id"]).body.some_event_data);
      }).Go();
  EXPECT_TRUE(WasCommitted(verify_event_added_result));

  EXPECT_EQ(0u, event_store.readonly_nonstorage_event_log_persister.Size());

  {
    EventOutsideStorage e;
    e.message = "haha";
    event_store.full_event_log.Publish(e);
  }

  while (event_store.readonly_nonstorage_event_log_persister.Size() < 1u) {
    ;  // Spin lock.
  }
  EXPECT_EQ(1u, event_store.readonly_nonstorage_event_log_persister.Size());
  EXPECT_EQ("haha",
            (*event_store.readonly_nonstorage_event_log_persister.Iterate(0u, 1u).begin()).entry.message);

  {
    event_store_t resumed_event_store(persistence_file_name);

    const auto verify_persisted_result =
        resumed_event_store.event_store_storage.Transaction([](ImmutableFields<db_t> fields) {
          EXPECT_EQ(1u, fields.events.Size());
          EXPECT_TRUE(Exists(fields.events["another_id"]));
          EXPECT_EQ("bar", Value(fields.events["another_id"]).body.some_event_data);
        }).Go();
    EXPECT_TRUE(WasCommitted(verify_persisted_result));
  }
}
