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

#define CURRENT_MOCK_TIME

#include "../test_helpers.cc"

#ifndef STORAGE_ONLY_RUN_RESTFUL_TESTS

TEST(TransactionalStorage, UseExternallyProvidedStream) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using storage_t = TestStorage<StreamInMemoryStreamPersister>;

  static_assert(
      std::is_same<
          typename storage_t::persister_t::stream_t,
          current::stream::Stream<typename storage_t::persister_t::transaction_t, current::persistence::Memory>>::value,
      "");

  auto storage = storage_t::CreateMasterStorage();

  {
    current::time::SetNow(std::chrono::microseconds(100));
    const auto result =
        storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
                 fields.d.Add(Record{"own_stream", 42});
               })
            .Go();
    EXPECT_TRUE(WasCommitted(result));
  }

  std::string collected;
  StorageStreamTestProcessor<storage_t::transaction_t> processor(collected);
  storage->Subscribe(processor);
  EXPECT_EQ(
      "{\"index\":0,\"us\":100}\t{\"meta\":{\"begin_us\":100,\"end_us\":100,\"fields\":{}},\"mutations\":[{"
      "\"RecordDictionaryUpdated\":{\"us\":100,\"data\":{\"lhs\":\"own_stream\",\"rhs\":42}},\"\":"
      "\"T9200018162904582576\"}]}\n",
      collected);
}

TEST(TransactionalStorage, ReplicationViaHTTP) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using storage_t = TestStorage<StreamStreamPersister>;

  // Create master storage.
  const std::string golden_storage_file_name =
      current::FileSystem::JoinPath("golden", "transactions_to_replicate.json");
  const std::string master_storage_file_name =
      current::FileSystem::JoinPath(FLAGS_transactional_storage_test_tmpdir, "data1");
  const auto master_storage_file_remover = current::FileSystem::ScopedRmFile(master_storage_file_name);

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));
  static_cast<void>(http_server);

  auto master_stream = storage_t::stream_t::CreateStream(master_storage_file_name);
  auto master_storage = storage_t::CreateMasterStorageAtopExistingStream(master_stream);
  master_storage->ExposeRawLogViaHTTP(port, "/raw_log");
  const std::string base_url = Printf("http://localhost:%d/raw_log", port);

  // Confirm the storage is available via HTTP.
  {
    const auto result = HTTP(GET(base_url + "?schema=json"));
    EXPECT_EQ(200, static_cast<int>(result.code));
    const auto result2 = HTTP(GET(base_url + "/schema.json"));
    EXPECT_EQ(200, static_cast<int>(result2.code));
    EXPECT_EQ(result.body, result2.body);
  }

  // Perform a couple transactions.
  {
    current::time::SetNow(std::chrono::microseconds(100));
    const auto result = master_storage
                            ->ReadWriteTransaction([](MutableFields<storage_t> fields) {
                              current::time::SetNow(std::chrono::microseconds(101));
                              fields.d.Add(Record{"one", 1});
                              current::time::SetNow(std::chrono::microseconds(102));
                              fields.d.Add(Record{"two", 2});
                              fields.SetTransactionMetaField("user", "dima");
                              current::time::SetNow(std::chrono::microseconds(103));
                            })
                            .Go();
    EXPECT_TRUE(WasCommitted(result));
  }
  {
    current::time::SetNow(std::chrono::microseconds(200));
    const auto result = master_storage
                            ->ReadWriteTransaction([](MutableFields<storage_t> fields) {
                              current::time::SetNow(std::chrono::microseconds(201));
                              fields.d.Add(Record{"three", 3});
                              current::time::SetNow(std::chrono::microseconds(202));
                              fields.d.Erase("two");
                              current::time::SetNow(std::chrono::microseconds(203));
                            })
                            .Go();
    EXPECT_TRUE(WasCommitted(result));
    current::time::SetNow(std::chrono::microseconds(300));
    master_storage->PublisherUsed()->UpdateHead();
  }

  // Confirm an empty transaction is not persisted.
  {
    current::time::SetNow(std::chrono::microseconds(302));
    master_storage->ReadWriteTransaction([](MutableFields<storage_t>) {}).Wait();
  }

  // Confirm the non-empty transactions have been persisted, while the empty one has been skipped.
  current::FileSystem::WriteStringToFile(current::FileSystem::ReadFileAsString(master_storage_file_name),
                                         golden_storage_file_name.c_str());
  EXPECT_EQ(current::FileSystem::ReadFileAsString(golden_storage_file_name),
            current::FileSystem::ReadFileAsString(master_storage_file_name));

  // Create stream for replication.
  const std::string replicated_stream_file_name =
      current::FileSystem::JoinPath(FLAGS_transactional_storage_test_tmpdir, "data2");
  const auto replicated_stream_file_remover = current::FileSystem::ScopedRmFile(replicated_stream_file_name);
  using transaction_t = typename storage_t::transaction_t;
  using stream_t = current::stream::Stream<transaction_t, current::persistence::File>;
  using replicator_t = current::stream::StreamReplicator<stream_t>;
  auto owned_replicated_stream(stream_t::CreateStream(replicated_stream_file_name));

  // Replicate data via subscription to master storage raw log.
  current::stream::SubscribableRemoteStream<transaction_t> remote_stream(Printf("http://localhost:%d/raw_log", port));
  EXPECT_TRUE(owned_replicated_stream->IsMasterStream());
  auto replicator = std::make_unique<replicator_t>(owned_replicated_stream);
  EXPECT_FALSE(owned_replicated_stream->IsMasterStream());

  // Create the storage atop the following stream.
  auto replicated_storage = storage_t::CreateFollowingStorageAtopExistingStream(owned_replicated_stream);
  EXPECT_FALSE(replicated_storage->IsMasterStorage());

  const current::Borrowed<stream_t> replicated_stream(replicated_storage->BorrowUnderlyingStream());

  {
    const auto subscriber_scope = remote_stream.Subscribe(*replicator);
    while (replicated_stream->Data()->CurrentHead() != std::chrono::microseconds(300)) {
      std::this_thread::yield();
    }
  }

  EXPECT_FALSE(replicated_storage->IsMasterStorage());
  // Return data authority to the stream as we completed the replication process.
  replicator = nullptr;
  EXPECT_FALSE(replicated_storage->IsMasterStorage());

  // Check that persisted files are the same.
  EXPECT_EQ(current::FileSystem::ReadFileAsString(master_storage_file_name),
            current::FileSystem::ReadFileAsString(replicated_stream_file_name));

  // Wait until the we can see the mutation made in the last transaction.
  while (replicated_storage->LastAppliedTimestamp() < std::chrono::microseconds(200)) {
    std::this_thread::yield();
  }

  // Test data consistency performing a transaction in the replicated storage.
  const auto result = replicated_storage
                          ->ReadOnlyTransaction([](ImmutableFields<storage_t> fields) {
                            EXPECT_EQ(2u, fields.d.Size());
                            EXPECT_EQ(1, Value(fields.d["one"]).rhs);
                            EXPECT_EQ(3, Value(fields.d["three"]).rhs);
                            EXPECT_FALSE(Exists(fields.d["two"]));
                          })
                          .Go();
  EXPECT_TRUE(WasCommitted(result));
}

namespace transactional_storage_test {

CURRENT_STRUCT(StreamEntryOutsideStorage) {
  CURRENT_FIELD(s, std::string);
  CURRENT_CONSTRUCTOR(StreamEntryOutsideStorage)(const std::string& s = "") : s(s) {}
};

}  // namespace transactional_storage_test

TEST(TransactionalStorage, UseExternallyProvidedStreamStreamOfBroaderType) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using pre_storage_t = TestStorage<StreamInMemoryStreamPersister>;
  using transaction_t = typename pre_storage_t::transaction_t;

  static_assert(std::is_same_v<transaction_t, typename pre_storage_t::persister_t::transaction_t>, "");

  using storage_t = TestStorage<StreamInMemoryStreamPersister,
                                current::storage::transaction_policy::Synchronous,
                                Variant<transaction_t, StreamEntryOutsideStorage>>;

  static_assert(std::is_same<typename storage_t::persister_t::stream_t,
                             current::stream::Stream<Variant<transaction_t, StreamEntryOutsideStorage>,
                                                     current::persistence::Memory>>::value,
                "");

  auto owned_stream = storage_t::stream_t::CreateStream();
  auto storage = storage_t::CreateMasterStorageAtopExistingStream(owned_stream);

  {// Add three records to the stream: first and third externally, second through the storage.
   {storage->PublisherUsed()->Publish(StreamEntryOutsideStorage("one"), std::chrono::microseconds(1));
}

{
  current::time::SetNow(std::chrono::microseconds(2));
  const auto result =
      storage->ReadWriteTransaction([](MutableFields<storage_t> fields) {
               fields.d.Add(Record{"two", 2});
             })
          .Go();
  EXPECT_TRUE(WasCommitted(result));
}
{ storage->PublisherUsed()->Publish(StreamEntryOutsideStorage("three"), std::chrono::microseconds(3)); }
}

{
  // Subscribe to and collect transactions.
  std::string collected_transactions;
  StorageStreamTestProcessor<transaction_t> processor(collected_transactions);
  processor.SetAllowTerminateOnOnMoreEntriesOfRightType();
  storage->Subscribe<transaction_t>(processor);
  EXPECT_EQ(
      "{\"index\":1,\"us\":2}\t"
      "{\"meta\":{\"begin_us\":2,\"end_us\":2,\"fields\":{}},\"mutations\":["
      "{\"RecordDictionaryUpdated\":{\"us\":2,\"data\":{\"lhs\":\"two\",\"rhs\":2}},"
      "\"\":\"T9200018162904582576\"}]}\n",
      collected_transactions);
}

{
  // Subscribe to and collect non-transactions.
  std::string collected_non_transactions;
  StorageStreamTestProcessor<StreamEntryOutsideStorage> processor(collected_non_transactions);
  processor.SetAllowTerminateOnOnMoreEntriesOfRightType();
  storage->Subscribe<StreamEntryOutsideStorage>(processor);
  EXPECT_EQ("{\"index\":0,\"us\":1}\t{\"s\":\"one\"}\n{\"index\":2,\"us\":3}\t{\"s\":\"three\"}\n",
            collected_non_transactions);
}

{
  // Confirm replaying storage with a mixed-content stream does its job.
  auto replayed = storage_t::CreateFollowingStorageAtopExistingStream(storage->UnderlyingStream());
  while (replayed->LastAppliedTimestamp() < std::chrono::microseconds(2)) {  // As `3` is a non-transaction.
    std::this_thread::yield();
  }
  const auto result = replayed
                          ->ReadOnlyTransaction([](ImmutableFields<storage_t> fields) {
                            EXPECT_EQ(1u, fields.d.Size());
                            ASSERT_TRUE(Exists(fields.d["two"]));
                            EXPECT_EQ(2, Value(fields.d["two"]).rhs);
                          })
                          .Go();
  EXPECT_TRUE(WasCommitted(result));
}
}

TEST(TransactionalStorage, FollowingStorageFlipsToMaster) {
  current::time::ResetToZero();

  using namespace transactional_storage_test;
  using storage_t = SimpleStorage<StreamStreamPersister>;
  using transaction_t = typename storage_t::transaction_t;
  using stream_t = current::stream::Stream<transaction_t, current::persistence::File>;
  using StreamReplicator = current::stream::StreamReplicator<stream_t>;

  const std::string master_file_name = current::FileSystem::JoinPath(FLAGS_transactional_storage_test_tmpdir, "master");
  const auto master_file_remover = current::FileSystem::ScopedRmFile(master_file_name);

  const std::string follower_file_name =
      current::FileSystem::JoinPath(FLAGS_transactional_storage_test_tmpdir, "follower");
  const auto follower_file_remover = current::FileSystem::ScopedRmFile(follower_file_name);

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));
  static_cast<void>(http_server);

  auto owned_follower_stream = stream_t::CreateStream(follower_file_name);
  // Replicator acquires the stream's persister object in its constructor.
  auto replicator = std::make_unique<StreamReplicator>(owned_follower_stream);

  // The underlying stream is created and owned by `master_storage`.
  auto master_stream = storage_t::stream_t::CreateStream(master_file_name);
  auto master_storage = storage_t::CreateMasterStorageAtopExistingStream(master_stream);

  // The followering storage is created atop a stream with external data authority.
  auto follower_storage = storage_t::CreateFollowingStorageAtopExistingStream(owned_follower_stream);

  EXPECT_TRUE(master_storage->IsMasterStorage());
  EXPECT_FALSE(follower_storage->IsMasterStorage());

  const auto base_url = current::strings::Printf("http://localhost:%d", port);

  // Start RESTful service atop follower storage.
  auto rest = RESTfulStorage<storage_t>(*follower_storage, port, "/api", "http://unittest.current.ai");

  // Launch the continuous replication process.
  {
    const auto replicator_scope = master_storage->template Subscribe<transaction_t>(*replicator);

    // Confirm an empty collection is returned.
    {
      const auto result = HTTP(GET(base_url + "/api/data/user"));
      EXPECT_EQ(200, static_cast<int>(result.code));
      EXPECT_EQ("", result.body);
    }

    // Publish one record.
    current::time::SetNow(std::chrono::microseconds(100));
    {
      const auto result =
          master_storage
              ->ReadWriteTransaction([](MutableFields<storage_t> fields) { fields.user.Add(SimpleUser("John", "JD")); })
              .Go();
      EXPECT_TRUE(WasCommitted(result));
    }

    // Wait until the transaction performed above is replicated to the `follower_stream` and imported by
    // the `follower_storage`.
    {
      size_t user_size;
      user_size = 0u;
      do {
        const auto result =
            follower_storage->ReadOnlyTransaction([](ImmutableFields<storage_t> fields) { return fields.user.Size(); })
                .Go();
        EXPECT_TRUE(WasCommitted(result));
        user_size = Value(result);
      } while (user_size == 0u);
      EXPECT_EQ(1u, user_size);
    }

    // Check that the the following storage now has the same record as the master one.
    {
      const auto result = follower_storage
                              ->ReadOnlyTransaction([](ImmutableFields<storage_t> fields) {
                                ASSERT_TRUE(Exists(fields.user["John"]));
                                EXPECT_EQ("JD", Value(fields.user["John"]).name);
                              })
                              .Go();
      EXPECT_TRUE(WasCommitted(result));
    }
    {
      const auto result = HTTP(GET(base_url + "/api/data/user/John"));
      EXPECT_EQ(200, static_cast<int>(result.code));
      EXPECT_EQ("JD", ParseJSON<SimpleUser>(result.body).name);
    }

    // Mutating access to the RESTful service of the `follower` is not allowed.
    {
      const auto post_response = HTTP(POST(base_url + "/api/data/user", SimpleUser("max", "MZ")));
      EXPECT_EQ(405, static_cast<int>(post_response.code));
      const auto put_response = HTTP(PUT(base_url + "/api/data/user/John", SimpleUser("John", "DJ")));
      EXPECT_EQ(405, static_cast<int>(post_response.code));
      const auto delete_response = HTTP(DELETE(base_url + "/api/data/user/John"));
      EXPECT_EQ(405, static_cast<int>(post_response.code));
    }

    // Attempt to run read-write transaction in `Follower` mode throws an exception.
    EXPECT_THROW(follower_storage->ReadWriteTransaction([](MutableFields<storage_t>) {}),
                 current::storage::ReadWriteTransactionInFollowerStorageException);
    EXPECT_THROW(follower_storage->ReadWriteTransaction([](MutableFields<storage_t>) { return 42; }, [](int) {}),
                 current::storage::ReadWriteTransactionInFollowerStorageException);

    // At this moment the content of both persisted files must be identical.
    EXPECT_EQ(current::FileSystem::ReadFileAsString(master_file_name),
              current::FileSystem::ReadFileAsString(follower_file_name));
  }

  // `FlipToMaster()` method on a storage with `Master` role throws an exception.
  EXPECT_TRUE(master_storage->IsMasterStorage());
  EXPECT_THROW(master_storage->FlipToMaster(), current::storage::StorageIsAlreadyMasterException);

  EXPECT_FALSE(follower_storage->IsMasterStorage());

  // `StreamReplicator` must be destroyed, otherwise `FlipToMaster()` will wait for its release forever.
  replicator = nullptr;

  EXPECT_TRUE(master_storage->IsMasterStorage());
  EXPECT_FALSE(follower_storage->IsMasterStorage());

  // Switch to a `Master` role.
  ASSERT_NO_THROW(follower_storage->FlipToMaster());

  // NOTE(dkorolev): This is not implemented yet, `master_storage` will remain the "master" of its own stream.
  // EXPECT_FALSE(master_storage->IsMasterStorage());  // <--- THIS IS UNIMPLEMENTED FOR NOW. -- D.K.
  EXPECT_TRUE(follower_storage->IsMasterStorage());

  // Publish record and check that everything goes well.
  current::time::SetNow(std::chrono::microseconds(200));
  {
    const auto result =
        follower_storage
            ->ReadWriteTransaction([](MutableFields<storage_t> fields) { fields.user.Add(SimpleUser("max", "MZ")); })
            .Go();
    EXPECT_TRUE(WasCommitted(result));
  }
  current::time::SetNow(std::chrono::microseconds(300));
  {
    // Publish one more record using REST.
    const auto post_response = HTTP(POST(base_url + "/api/data/user", SimpleUser("dima", "DK")));
    EXPECT_EQ(201, static_cast<int>(post_response.code));
    const auto user_key = post_response.body;

    // Ensure that all the records are in place.
    const auto result = follower_storage
                            ->ReadOnlyTransaction([user_key](ImmutableFields<storage_t> fields) {
                              EXPECT_EQ(3u, fields.user.Size());
                              ASSERT_TRUE(Exists(fields.user["max"]));
                              EXPECT_EQ("MZ", Value(fields.user["max"]).name);
                              ASSERT_TRUE(Exists(fields.user[user_key]));
                              EXPECT_EQ("DK", Value(fields.user[user_key]).name);
                            })
                            .Go();
    EXPECT_TRUE(WasCommitted(result));
  }
}

#endif  // STORAGE_ONLY_RUN_RESTFUL_TESTS
