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

#ifndef CURRENT_STORAGE_DOCU_DOCU_2_CODE_CC
#define CURRENT_STORAGE_DOCU_DOCU_2_CODE_CC

#define CURRENT_MOCK_TIME

#include "../../port.h"

#include "../storage.h"
#include "../persister/sherlock.h"

#include "../../Bricks/file/file.h"
#include "../../Bricks/dflags/dflags.h"

#include "../../3rdparty/gtest/gtest.h"

DEFINE_string(storage_example_test_dir,
              ".current",
              "Local path for the test to create temporary files in.");

DEFINE_string(storage_example_file_name,
              "persisted",
              "The file name to store persisted entries in.");

namespace storage_docu {

CURRENT_ENUM(UserID, uint64_t) { INVALID = 0ull };

}  // namespace storage_docu

namespace storage_docu {

CURRENT_STRUCT(User) {
  CURRENT_FIELD(key, UserID);

  CURRENT_FIELD(name, std::string, "John Doe");
  
  CURRENT_FIELD(white, bool, true);
  CURRENT_FIELD(straight, bool, true);
  CURRENT_FIELD(male, bool, true);

  CURRENT_CONSTRUCTOR(User)(UserID key = UserID::INVALID) : key(key) {}
};

CURRENT_STORAGE_FIELD_ENTRY(UnorderedDictionary, User, PersistedUser);

CURRENT_STORAGE(ExampleStorageDefinition) {
  CURRENT_STORAGE_FIELD(users, PersistedUser);
};

}  // namespace storage_docu

TEST(StorageDocumentation, BasicInMemoryUsage) {
  current::time::ResetToZero();

  using namespace storage_docu;
  using ExampleStorage = ExampleStorageDefinition<SherlockInMemoryStreamPersister>;

  {
    ExampleStorage storage;
    EXPECT_EQ(1u, storage.FIELDS_COUNT);

    // Add two users.
    current::time::SetNow(std::chrono::microseconds(1));
    const auto result1 = storage.ReadWriteTransaction([](MutableFields<ExampleStorage> data) {
      EXPECT_TRUE(data.users.Empty());
      User alice;
      alice.name = "Alice";
      alice.key = static_cast<UserID>(101);
      alice.male = false;
      User bob;
      bob.name = "Bob";
      bob.key = static_cast<UserID>(102);
      data.users.Add(alice);
      data.users.Add(bob);
    }).Go();
    EXPECT_TRUE(WasCommitted(result1));

    // Delete one, but rollback the transaction.
    current::time::SetNow(std::chrono::microseconds(2));
    const auto result2 = storage.ReadWriteTransaction([](MutableFields<ExampleStorage> data) {
      EXPECT_FALSE(data.users.Empty());
      EXPECT_EQ(2u, data.users.Size());
      data.users.Erase(static_cast<UserID>(101));
      EXPECT_EQ(1u, data.users.Size());
      CURRENT_STORAGE_THROW_ROLLBACK();
    }).Go();
    EXPECT_FALSE(WasCommitted(result2));

    // Confirm the previous transaction was reverted, and delete the privileged user for real now.
    current::time::SetNow(std::chrono::microseconds(3));
    const auto result3 = storage.ReadWriteTransaction([](MutableFields<ExampleStorage> data) {
      EXPECT_FALSE(data.users.Empty());
      EXPECT_EQ(2u, data.users.Size());
      current::time::SetNow(std::chrono::microseconds(4));
      data.users.Erase(static_cast<UserID>(102));
      EXPECT_EQ(1u, data.users.Size());
    }).Go();
    EXPECT_TRUE(WasCommitted(result3));

    // Confirm the non-reverted deleted user was indeed deleted.
    current::time::SetNow(std::chrono::microseconds(5));
    const auto result4 = storage.ReadOnlyTransaction([](ImmutableFields<ExampleStorage> data) {
      EXPECT_FALSE(data.users.Empty());
      EXPECT_EQ(1u, data.users.Size());

      EXPECT_TRUE(Exists(data.users[static_cast<UserID>(101)]));
      EXPECT_EQ("Alice", Value(data.users[static_cast<UserID>(101)]).name);
      EXPECT_FALSE(Value(data.users[static_cast<UserID>(101)]).male);

      EXPECT_FALSE(Exists(data.users[static_cast<UserID>(102)]));
    }).Go();
    EXPECT_TRUE(WasCommitted(result4));
  }
}

TEST(StorageDocumentation, BasicUsage) {
  current::time::ResetToZero();

  using namespace storage_docu;
  using current::storage::TransactionMetaFields;
  using ExampleStorage = ExampleStorageDefinition<SherlockStreamPersister>;

  const std::string persistence_file_name =
      current::FileSystem::JoinPath(FLAGS_storage_example_test_dir, FLAGS_storage_example_file_name);

  const auto persistence_file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

  {
    EXPECT_EQ(1u, ExampleStorage::FIELDS_COUNT);
    ExampleStorage storage(persistence_file_name);

    current::time::SetNow(std::chrono::microseconds(1001ull));
    // TODO(dkorolev) + TODO(mzhurovich): Use the return value of `.Transaction(...)`.
    const auto result1 = storage.ReadWriteTransaction([](MutableFields<ExampleStorage> data) {
      User test1;
      User test2;
      test1.key = static_cast<UserID>(1);
      test1.name = "test1";
      test2.key = static_cast<UserID>(2);
      test2.name = "test2";
      test2.straight = false;
      data.users.Add(test1);
      data.users.Add(test2);
      data.SetTransactionMetaField("user", "vasya");
      data.EraseTransactionMetaField("user");
      data.SetTransactionMetaField("user", "max");
      current::time::SetNow(std::chrono::microseconds(1002ull));  // <-- This timestamp will be used.
    }).Go();
    EXPECT_TRUE(WasCommitted(result1));

    current::time::SetNow(std::chrono::microseconds(1003ull));
    const auto result2 = storage.ReadWriteTransaction([](MutableFields<ExampleStorage> data) {
      User test3;
      test3.key = static_cast<UserID>(3);
      test3.name = "to be deleted";
      data.users.Add(test3);
      current::time::SetNow(std::chrono::microseconds(1004ull));
    }).Go();
    EXPECT_TRUE(WasCommitted(result2));

    current::time::SetNow(std::chrono::microseconds(1005ull));
    const auto result3 = storage.ReadWriteTransaction([](MutableFields<ExampleStorage> data) {
      data.users.Erase(static_cast<UserID>(3));
      current::time::SetNow(std::chrono::microseconds(1006ull));
    }).Go();
    EXPECT_TRUE(WasCommitted(result3));
  }

  const std::vector<std::string> persisted_transactions =
    current::strings::Split<current::strings::ByLines>(
      current::FileSystem::ReadFileAsString(persistence_file_name));

  ASSERT_EQ(3u, persisted_transactions.size());
  const auto ParseAndValidateRow = [](const std::string& line, uint64_t index, std::chrono::microseconds timestamp) {
    std::istringstream iss(line);
    const size_t tab_pos = line.find('\t');
    const auto persisted_idx_ts = ParseJSON<idxts_t>(line.substr(0, tab_pos));
    EXPECT_EQ(index, persisted_idx_ts.index);
    EXPECT_EQ(timestamp, persisted_idx_ts.us);
    return ParseJSON<ExampleStorage::transaction_t>(line.substr(tab_pos + 1));
  };

  {
    const auto t = ParseAndValidateRow(persisted_transactions[0], 0u, std::chrono::microseconds(1002));
    ASSERT_EQ(2u, t.mutations.size());

    ASSERT_TRUE(Exists<PersistedUserUpdated>(t.mutations[0]));
    EXPECT_EQ("test1", Value<PersistedUserUpdated>(t.mutations[0]).data.name);
    EXPECT_TRUE(Value<PersistedUserUpdated>(t.mutations[0]).data.straight);

    ASSERT_TRUE(Exists<PersistedUserUpdated>(t.mutations[1]));
    EXPECT_EQ("test2", Value<PersistedUserUpdated>(t.mutations[1]).data.name);
    EXPECT_FALSE(Value<PersistedUserUpdated>(t.mutations[1]).data.straight);

    EXPECT_EQ(1002, static_cast<int>(t.meta.timestamp.count()));
    EXPECT_EQ(1u, t.meta.fields.size());
    EXPECT_EQ("max", t.meta.fields.at("user"));
  }

  {
    const auto t = ParseAndValidateRow(persisted_transactions[1], 1u, std::chrono::microseconds(1004));
    ASSERT_EQ(1u, t.mutations.size());

    ASSERT_TRUE(Exists<PersistedUserUpdated>(t.mutations[0]));
    EXPECT_EQ("to be deleted", Value<PersistedUserUpdated>(t.mutations[0]).data.name);

    EXPECT_EQ(1004, static_cast<int>(t.meta.timestamp.count()));
  }

  {
    const auto t = ParseAndValidateRow(persisted_transactions[2], 2u, std::chrono::microseconds(1006));
    ASSERT_EQ(1u, t.mutations.size());

    ASSERT_FALSE(Exists<PersistedUserUpdated>(t.mutations[0]));
    ASSERT_TRUE(Exists<PersistedUserDeleted>(t.mutations[0]));
    EXPECT_EQ(3, static_cast<int>(Value<PersistedUserDeleted>(t.mutations[0]).key));

    EXPECT_EQ(1006, static_cast<int>(t.meta.timestamp.count()));
  }

  {
    ExampleStorage replayed(persistence_file_name);
    const auto result = replayed.ReadOnlyTransaction([](ImmutableFields<ExampleStorage> data) {
      EXPECT_EQ(2u, data.users.Size());
      EXPECT_TRUE(Exists(data.users[static_cast<UserID>(1)]));
      EXPECT_EQ("test1", Value(data.users[static_cast<UserID>(1)]).name);
      EXPECT_TRUE(Exists(data.users[static_cast<UserID>(2)]));
      EXPECT_EQ("test2", Value(data.users[static_cast<UserID>(2)]).name);
    }).Go();
    EXPECT_TRUE(WasCommitted(result));
  }
}

#endif  // CURRENT_STORAGE_DOCU_DOCU_2_CODE_CC
