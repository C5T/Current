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

#define CURRENT_MOCK_TIME

#include "event_store.h"
#include "schema.h"

#include "../../bricks/dflags/dflags.h"
#include "../../bricks/file/file.h"

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

#ifndef CURRENT_WINDOWS
DEFINE_string(event_store_test_tmpdir, ".current", "Local path for the test to create temporary files in.");
#else
DEFINE_string(event_store_test_tmpdir, ".", "Local path for the test to create temporary files in.");
#endif

TEST(EventStore, SmokeWithInMemoryEventStore) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));
  static_cast<void>(http_server);

  current::time::ResetToZero();

  using event_store_t = EventStore<EventStoreDB, Event, EventOutsideStorage, StreamInMemoryStreamPersister>;
  using db_t = event_store_t::event_store_storage_t;

  event_store_t event_store(port, "");

  EXPECT_EQ(0u, event_store.readonly_nonstorage_event_log_persister->Size());

  const auto add_event_result = event_store.event_store_storage
                                    ->ReadWriteTransaction([](MutableFields<db_t> fields) {
                                      EXPECT_TRUE(fields.events.Empty());
                                      Event event;
                                      event.key = "id";
                                      event.body.some_event_data = "foo";
                                      fields.events.Add(event);
                                    })
                                    .Go();
  EXPECT_TRUE(WasCommitted(add_event_result));

  const auto verify_event_added_result = event_store.event_store_storage
                                             ->ReadOnlyTransaction([](ImmutableFields<db_t> fields) {
                                               EXPECT_EQ(1u, fields.events.Size());
                                               EXPECT_TRUE(Exists(fields.events["id"]));
                                               EXPECT_EQ("foo", Value(fields.events["id"]).body.some_event_data);
                                             })
                                             .Go();
  EXPECT_TRUE(WasCommitted(verify_event_added_result));

  EXPECT_EQ(0u, event_store.readonly_nonstorage_event_log_persister->Size());

  {
    EventOutsideStorage e;
    e.message = "haha";
    event_store.event_store_storage->PublisherUsed()->Publish(e);
  }

  while (event_store.readonly_nonstorage_event_log_persister->Size() < 1u) {
    std::this_thread::yield();
  }
  EXPECT_EQ(1u, event_store.readonly_nonstorage_event_log_persister->Size());
  EXPECT_EQ("haha", (*event_store.readonly_nonstorage_event_log_persister->Iterate(0u, 1u).begin()).entry.message);
}

TEST(EventStore, SmokeWithDiskPersistedEventStore) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));
  static_cast<void>(http_server);

  current::time::ResetToZero();

  const std::string persistence_file_name =
      current::FileSystem::JoinPath(FLAGS_event_store_test_tmpdir, ".current_testdb");
  const auto persistence_file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

  using event_store_t = EventStore<EventStoreDB, Event, EventOutsideStorage, StreamStreamPersister>;
  using db_t = event_store_t::event_store_storage_t;

  {
    event_store_t event_store(port, "", persistence_file_name);

    EXPECT_EQ(0u, event_store.readonly_nonstorage_event_log_persister->Size());

    const auto add_event_result = event_store.event_store_storage
                                      ->ReadWriteTransaction([](MutableFields<db_t> fields) {
                                        EXPECT_TRUE(fields.events.Empty());
                                        Event event;
                                        event.key = "another_id";
                                        event.body.some_event_data = "bar";
                                        fields.events.Add(event);
                                      })
                                      .Go();
    EXPECT_TRUE(WasCommitted(add_event_result));

    const auto verify_event_added_result =
        event_store.event_store_storage
            ->ReadOnlyTransaction([](ImmutableFields<db_t> fields) {
              EXPECT_EQ(1u, fields.events.Size());
              EXPECT_TRUE(Exists(fields.events["another_id"]));
              EXPECT_EQ("bar", Value(fields.events["another_id"]).body.some_event_data);
            })
            .Go();
    EXPECT_TRUE(WasCommitted(verify_event_added_result));

    EXPECT_EQ(0u, event_store.readonly_nonstorage_event_log_persister->Size());

    {
      EventOutsideStorage e;
      e.message = "haha";
      event_store.event_store_storage->PublisherUsed()->Publish(e);
    }

    while (event_store.readonly_nonstorage_event_log_persister->Size() < 1u) {
      std::this_thread::yield();
    }
    EXPECT_EQ(1u, event_store.readonly_nonstorage_event_log_persister->Size());
    EXPECT_EQ("haha", (*event_store.readonly_nonstorage_event_log_persister->Iterate(0u, 1u).begin()).entry.message);
  }

  {
    event_store_t resumed_event_store(port, "", persistence_file_name);

    const auto verify_persisted_result =
        resumed_event_store.event_store_storage
            ->ReadOnlyTransaction([](ImmutableFields<db_t> fields) {
              EXPECT_EQ(1u, fields.events.Size());
              EXPECT_TRUE(Exists(fields.events["another_id"]));
              EXPECT_EQ("bar", Value(fields.events["another_id"]).body.some_event_data);
            })
            .Go();
    EXPECT_TRUE(WasCommitted(verify_persisted_result));
  }
}

TEST(EventStore, SmokeWithHTTP) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));
  static_cast<void>(http_server);

  current::time::ResetToZero();

  using event_store_t = EventStore<EventStoreDB, Event, EventOutsideStorage, StreamInMemoryStreamPersister>;
  using db_t = event_store_t::event_store_storage_t;

  event_store_t event_store(port, "");

  EXPECT_EQ("UP!\n", HTTP(GET(Printf("http://localhost:%d/up", port))).body);

  EXPECT_EQ(404, static_cast<int>(HTTP(GET(Printf("http://localhost:%d/event/http1", port))).code));

  EXPECT_EQ(0u, event_store.readonly_nonstorage_event_log_persister->Size());

  const auto add_event_result = event_store.event_store_storage
                                    ->ReadWriteTransaction([](MutableFields<db_t> fields) {
                                      EXPECT_TRUE(fields.events.Empty());
                                      Event event;
                                      event.key = "http1";
                                      event.body.some_event_data = "yeah1";
                                      fields.events.Add(event);
                                    })
                                    .Go();
  EXPECT_TRUE(WasCommitted(add_event_result));

  {
    const auto result = HTTP(GET(Printf("http://localhost:%d/event/http1", port)));
    EXPECT_EQ(200, static_cast<int>(result.code));
    EXPECT_EQ("{\"key\":\"http1\",\"body\":{\"some_event_data\":\"yeah1\"}}\n", result.body);
  }

  EXPECT_EQ(0u, event_store.readonly_nonstorage_event_log_persister->Size());
  EXPECT_EQ("0\n", HTTP(GET(Printf("http://localhost:%d/subscribe?sizeonly", port))).body);

  current::time::SetNow(std::chrono::microseconds(42000), std::chrono::microseconds(42100));
  {
    Event event2;
    event2.key = "http2";
    event2.body.some_event_data = "yeah2";
    // Corresponding stream entry will have timestamp = `42003`, since Storage makes several `Now()` calls
    // to timestamp transaction begin/end as well as the mutation itself.
    EXPECT_EQ(201, static_cast<int>(HTTP(POST(Printf("http://localhost:%d/event", port), event2)).code));
  }

  while (event_store.readonly_nonstorage_event_log_persister->Size() < 1u) {
    std::this_thread::yield();
  }
  EXPECT_EQ(1u, event_store.readonly_nonstorage_event_log_persister->Size());
  EXPECT_EQ("1\n", HTTP(GET(Printf("http://localhost:%d/subscribe?sizeonly", port))).body);

  const auto verify_http_event_added_result =
      event_store.event_store_storage
          ->ReadOnlyTransaction([](ImmutableFields<db_t> fields) {
            EXPECT_EQ(2u, fields.events.Size());
            EXPECT_TRUE(Exists(fields.events["http2"]));
            EXPECT_EQ("yeah2", Value(fields.events["http2"]).body.some_event_data);
          })
          .Go();
  EXPECT_TRUE(WasCommitted(verify_http_event_added_result));

  ASSERT_EQ(1u, event_store.readonly_nonstorage_event_log_persister->Size());
  EXPECT_EQ("Event added: http2",
            (*event_store.readonly_nonstorage_event_log_persister->Iterate(0u, 1u).begin()).entry.message);
  // `42003` as the non-storage event is published after the storage one.
  EXPECT_EQ("{\"index\":0,\"us\":42003}\t{\"message\":\"Event added: http2\"}\n",
            HTTP(GET(Printf("http://localhost:%d/subscribe?i=0&n=1", port))).body);
}
