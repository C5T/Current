/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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

#include "../../port.h"

#include <string>

#define CURRENT_MOCK_TIME  // `SetNow()`.

#include "memory.h"
#include "file.h"

#include "../ss/ss.h"

#include "../../bricks/dflags/dflags.h"
#include "../../bricks/file/file.h"
#include "../../bricks/strings/join.h"
#include "../../bricks/strings/printf.h"

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_string(persistence_test_tmpdir, ".current", "Local path for the test to create temporary files in.");

namespace persistence_test {

using current::strings::Join;
using current::strings::Printf;

CURRENT_STRUCT(StorableString) {
  CURRENT_FIELD(s, std::string, "");
  CURRENT_DEFAULT_CONSTRUCTOR(StorableString) {}
  CURRENT_CONSTRUCTOR(StorableString)(const std::string& s) : s(s) {}
};

}  // namespace persistence_test

TEST(PersistenceLayer, Memory) {
  current::time::ResetToZero();

  using namespace persistence_test;

  using IMPL = current::persistence::Memory<std::string>;

  const auto namespace_name = current::ss::StreamNamespaceName("namespace", "entry_name");

  {
    std::mutex mutex;
    IMPL impl(mutex, namespace_name);
    EXPECT_EQ(0u, impl.Size());

    impl.Publish("foo", std::chrono::microseconds(100));
    impl.Publish("bar", std::chrono::microseconds(200));
    current::time::SetNow(std::chrono::microseconds(300));
    EXPECT_EQ(2u, impl.Size());

    {
      std::vector<std::string> first_two;
      for (const auto& e : impl.Iterate()) {
        first_two.push_back(Printf(
            "%s %d %d", e.entry.c_str(), static_cast<int>(e.idx_ts.index), static_cast<int>(e.idx_ts.us.count())));
      }
      EXPECT_EQ("foo 0 100,bar 1 200", Join(first_two, ","));
      std::vector<std::string> first_two_unsafe;
      for (const auto& e : impl.IterateUnsafe()) {
        first_two_unsafe.push_back(e);
      }
      EXPECT_EQ(
          "{\"index\":0,\"us\":100}\t\"foo\","
          "{\"index\":1,\"us\":200}\t\"bar\"",
          Join(first_two_unsafe, ","));
    }

    impl.Publish("meh");
    EXPECT_EQ(3u, impl.Size());

    {
      std::vector<std::string> all_three;
      for (const auto& e : impl.Iterate()) {
        all_three.push_back(Printf(
            "%s %d %d", e.entry.c_str(), static_cast<int>(e.idx_ts.index), static_cast<int>(e.idx_ts.us.count())));
      }
      EXPECT_EQ("foo 0 100,bar 1 200,meh 2 300", Join(all_three, ","));
      std::vector<std::string> all_three_unsafe;
      for (const auto& e : impl.IterateUnsafe()) {
        all_three_unsafe.push_back(e);
      }
      EXPECT_EQ(
          "{\"index\":0,\"us\":100}\t\"foo\","
          "{\"index\":1,\"us\":200}\t\"bar\","
          "{\"index\":2,\"us\":300}\t\"meh\"",
          Join(all_three_unsafe, ","));
    }

    {
      std::vector<std::string> just_the_last_one;
      for (const auto& e : impl.Iterate(2)) {
        just_the_last_one.push_back(e.entry);
      }
      EXPECT_EQ("meh", Join(just_the_last_one, ","));
      std::vector<std::string> just_the_last_one_unsafe;
      for (const auto& e : impl.IterateUnsafe(2)) {
        just_the_last_one_unsafe.push_back(e);
      }
      EXPECT_EQ("{\"index\":2,\"us\":300}\t\"meh\"", Join(just_the_last_one_unsafe, ","));
    }

    {
      std::vector<std::string> just_the_last_one;
      for (const auto& e : impl.Iterate(std::chrono::microseconds(300))) {
        just_the_last_one.push_back(e.entry);
      }
      EXPECT_EQ("meh", Join(just_the_last_one, ","));
      std::vector<std::string> just_the_last_one_unsafe;
      for (const auto& e : impl.IterateUnsafe(std::chrono::microseconds(300))) {
        just_the_last_one_unsafe.push_back(e);
      }
      EXPECT_EQ("{\"index\":2,\"us\":300}\t\"meh\"", Join(just_the_last_one_unsafe, ","));
    }
  }

  {
    // Obviously, no state is shared for `Memory` implementation.
    // The data starts from ground zero.
    std::mutex mutex;
    IMPL impl(mutex, namespace_name);
    EXPECT_EQ(0u, impl.Size());
  }
}

TEST(PersistenceLayer, MemoryExceptions) {
  using namespace persistence_test;

  using IMPL = current::persistence::Memory<std::string>;

  static_assert(current::ss::IsPersister<IMPL>::value, "");
  static_assert(current::ss::IsEntryPersister<IMPL, std::string>::value, "");

  static_assert(!current::ss::IsPublisher<IMPL>::value, "");
  static_assert(!current::ss::IsEntryPublisher<IMPL, std::string>::value, "");
  static_assert(!current::ss::IsStreamPublisher<IMPL, std::string>::value, "");

  static_assert(!current::ss::IsPublisher<int>::value, "");
  static_assert(!current::ss::IsEntryPublisher<IMPL, int>::value, "");
  static_assert(!current::ss::IsStreamPublisher<IMPL, int>::value, "");

  static_assert(!current::ss::IsPersister<int>::value, "");
  static_assert(!current::ss::IsEntryPersister<IMPL, int>::value, "");

  const auto namespace_name = current::ss::StreamNamespaceName("namespace", "entry_name");

  {
    current::time::ResetToZero();
    // Time goes back.
    std::mutex mutex;
    IMPL impl(mutex, namespace_name);
    impl.Publish("2", std::chrono::microseconds(2));
    current::time::ResetToZero();
    current::time::SetNow(std::chrono::microseconds(1));
    ASSERT_THROW(impl.Publish("1"), current::ss::InconsistentTimestampException);
    ASSERT_THROW(impl.UpdateHead(), current::ss::InconsistentTimestampException);
    impl.UpdateHead(std::chrono::microseconds(4));
    current::time::SetNow(std::chrono::microseconds(3));
    ASSERT_THROW(impl.Publish("1"), current::ss::InconsistentTimestampException);
    ASSERT_THROW(impl.UpdateHead(), current::ss::InconsistentTimestampException);
  }

  {
    current::time::ResetToZero();
    // Time staying the same is as bad as time going back.
    current::time::SetNow(std::chrono::microseconds(3));
    std::mutex mutex;
    IMPL impl(mutex, namespace_name);
    impl.Publish("2");
    ASSERT_THROW(impl.Publish("1"), current::ss::InconsistentTimestampException);
    ASSERT_THROW(impl.UpdateHead(), current::ss::InconsistentTimestampException);
    current::time::SetNow(std::chrono::microseconds(4));
    impl.UpdateHead();
    ASSERT_THROW(impl.Publish("1"), current::ss::InconsistentTimestampException);
    ASSERT_THROW(impl.UpdateHead(), current::ss::InconsistentTimestampException);
  }

  {
    std::mutex mutex;
    IMPL impl(mutex, namespace_name);
    ASSERT_THROW(impl.LastPublishedIndexAndTimestamp(), current::persistence::NoEntriesPublishedYet);
  }

  {
    current::time::ResetToZero();
    std::mutex mutex;
    IMPL impl(mutex, namespace_name);
    impl.Publish("1", std::chrono::microseconds(1));
    impl.Publish("2", std::chrono::microseconds(2));
    impl.Publish("3", std::chrono::microseconds(3));
    ASSERT_THROW(impl.Iterate(1, 0), current::persistence::InvalidIterableRangeException);
    ASSERT_THROW(impl.IterateUnsafe(1, 0), current::persistence::InvalidIterableRangeException);
    ASSERT_THROW(impl.Iterate(100, 101), current::persistence::InvalidIterableRangeException);
    ASSERT_THROW((impl.IterateUnsafe(100, 101)), current::persistence::InvalidIterableRangeException);
    ASSERT_THROW(impl.Iterate(100, 100), current::persistence::InvalidIterableRangeException);
    ASSERT_THROW((impl.IterateUnsafe(100, 100)), current::persistence::InvalidIterableRangeException);
  }
}

TEST(PersistenceLayer, MemoryIteratorCanNotOutliveMemoryBlock) {
  using namespace persistence_test;
  using IMPL = current::persistence::Memory<std::string>;

  std::mutex mutex;
  const auto namespace_name = current::ss::StreamNamespaceName("namespace", "entry_name");
  auto p = std::make_unique<IMPL>(mutex, namespace_name);
  p->Publish("1", std::chrono::microseconds(1));
  p->Publish("2", std::chrono::microseconds(2));
  p->Publish("3", std::chrono::microseconds(3));

  std::thread t;  // To wait for the persister to shut down as iterators over it are done.

  {
    auto iterable = p->Iterate();
    auto iterable_unsafe = p->IterateUnsafe();
    EXPECT_TRUE(static_cast<bool>(iterable));
    EXPECT_TRUE(static_cast<bool>(iterable_unsafe));
    auto iterator = iterable.begin();
    auto iterator_unsafe = iterable_unsafe.begin();
    EXPECT_TRUE(static_cast<bool>(iterator));
    EXPECT_TRUE(static_cast<bool>(iterator_unsafe));
    EXPECT_EQ("1", (*iterator).entry);
    EXPECT_EQ("{\"index\":0,\"us\":1}\t\"1\"", *iterator_unsafe);

    t = std::thread([&p]() {
      // Release the persister. Well, begin to, as this "call" would block until the iterators are done.
      p = nullptr;
    });

    // Spin lock, and w/o a mutex it would hang with `NDEBUG=1`.
    {
      std::mutex mutex;
      while (true) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!iterator && !iterator_unsafe) {
          break;
        }
        std::this_thread::yield();
      }
    }

    ASSERT_FALSE(static_cast<bool>(iterator));         // Should no longer be available.
    ASSERT_FALSE(static_cast<bool>(iterator_unsafe));  // Should no longer be available.

    // Spin lock, and w/o a mutex it would hang with `NDEBUG=1`.
    {
      std::mutex mutex;
      while (true) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!iterable && !iterable_unsafe) {
          break;
        }
        std::this_thread::yield();
      }
    }

    ASSERT_FALSE(static_cast<bool>(iterable));         // Should no longer be available.
    ASSERT_FALSE(static_cast<bool>(iterable_unsafe));  // Should no longer be available.
  }

  t.join();
}

TEST(PersistenceLayer, File) {
  current::time::ResetToZero();

  using namespace persistence_test;

  using IMPL = current::persistence::File<StorableString>;

  const auto namespace_name = current::ss::StreamNamespaceName("namespace", "entry_name");
  const std::string persistence_file_name = current::FileSystem::JoinPath(FLAGS_persistence_test_tmpdir, "data");
  const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

  {
    std::mutex mutex;
    IMPL impl(mutex, namespace_name, persistence_file_name);
    EXPECT_EQ(0u, impl.Size());
    current::time::SetNow(std::chrono::microseconds(100));
    impl.Publish(StorableString("foo"));
    current::time::SetNow(std::chrono::microseconds(200));
    impl.Publish(StorableString("bar"));
    EXPECT_EQ(2u, impl.Size());
    current::time::SetNow(std::chrono::microseconds(300));
    EXPECT_EQ(200, impl.CurrentHead().count());
    impl.UpdateHead();
    EXPECT_EQ(300, impl.CurrentHead().count());

    {
      std::vector<std::string> first_two;
      for (const auto& e : impl.Iterate()) {
        first_two.push_back(Printf(
            "%s %d %d", e.entry.s.c_str(), static_cast<int>(e.idx_ts.index), static_cast<int>(e.idx_ts.us.count())));
      }
      EXPECT_EQ("foo 0 100,bar 1 200", Join(first_two, ","));
      std::vector<std::string> first_two_unsafe;
      for (const auto& e : impl.IterateUnsafe()) {
        first_two_unsafe.push_back(e);
      }
      EXPECT_EQ(
          "{\"index\":0,\"us\":100}\t{\"s\":\"foo\"},"
          "{\"index\":1,\"us\":200}\t{\"s\":\"bar\"}",
          Join(first_two_unsafe, ","));
    }

    current::time::SetNow(std::chrono::microseconds(500));
    impl.Publish(StorableString("meh"));
    EXPECT_EQ(3u, impl.Size());

    current::time::SetNow(std::chrono::microseconds(550));
    EXPECT_EQ(500, impl.CurrentHead().count());
    impl.UpdateHead();
    EXPECT_EQ(550, impl.CurrentHead().count());
    current::time::SetNow(std::chrono::microseconds(600));
    impl.UpdateHead();
    EXPECT_EQ(600, impl.CurrentHead().count());

    {
      std::vector<std::string> all_three;
      for (const auto& e : impl.Iterate()) {
        all_three.push_back(Printf(
            "%s %d %d", e.entry.s.c_str(), static_cast<int>(e.idx_ts.index), static_cast<int>(e.idx_ts.us.count())));
      }
      EXPECT_EQ("foo 0 100,bar 1 200,meh 2 500", Join(all_three, ","));
      std::vector<std::string> all_three_unsafe;
      for (const auto& e : impl.IterateUnsafe()) {
        all_three_unsafe.push_back(e);
      }
      EXPECT_EQ(
          "{\"index\":0,\"us\":100}\t{\"s\":\"foo\"},"
          "{\"index\":1,\"us\":200}\t{\"s\":\"bar\"},"
          "{\"index\":2,\"us\":500}\t{\"s\":\"meh\"}",
          Join(all_three_unsafe, ","));
    }
  }

  current::reflection::StructSchema struct_schema;
  struct_schema.AddType<StorableString>();
  const std::string signature =
      "#signature " + JSON(current::ss::StreamSignature(namespace_name, struct_schema.GetSchemaInfo())) + '\n';
  EXPECT_EQ(signature +
                "{\"index\":0,\"us\":100}\t{\"s\":\"foo\"}\n"
                "{\"index\":1,\"us\":200}\t{\"s\":\"bar\"}\n"
                "#head 00000000000000000300\n"
                "{\"index\":2,\"us\":500}\t{\"s\":\"meh\"}\n"
                "#head 00000000000000000600\n",
            current::FileSystem::ReadFileAsString(persistence_file_name));

  {
    // Confirm the data has been saved and can be replayed.
    std::mutex mutex;
    IMPL impl(mutex, namespace_name, persistence_file_name);
    EXPECT_EQ(3u, impl.Size());

    {
      std::vector<std::string> all_three;
      for (const auto& e : impl.Iterate()) {
        all_three.push_back(Printf(
            "%s %d %d", e.entry.s.c_str(), static_cast<int>(e.idx_ts.index), static_cast<int>(e.idx_ts.us.count())));
      }
      EXPECT_EQ("foo 0 100,bar 1 200,meh 2 500", Join(all_three, ","));
      std::vector<std::string> all_three_unsafe;
      for (const auto& e : impl.IterateUnsafe()) {
        all_three_unsafe.push_back(e);
      }
      EXPECT_EQ(
          "{\"index\":0,\"us\":100}\t{\"s\":\"foo\"},"
          "{\"index\":1,\"us\":200}\t{\"s\":\"bar\"},"
          "{\"index\":2,\"us\":500}\t{\"s\":\"meh\"}",
          Join(all_three_unsafe, ","));
    }

    current::time::SetNow(std::chrono::microseconds(998));
    EXPECT_EQ(600, impl.CurrentHead().count());
    impl.UpdateHead();
    EXPECT_EQ(998, impl.CurrentHead().count());
    current::time::SetNow(std::chrono::microseconds(999));
    impl.Publish(StorableString("blah"));
    EXPECT_EQ(4u, impl.Size());
    EXPECT_EQ(999, impl.CurrentHead().count());

    {
      std::vector<std::string> all_four;
      for (const auto& e : impl.Iterate()) {
        all_four.push_back(Printf(
            "%s %d %d", e.entry.s.c_str(), static_cast<int>(e.idx_ts.index), static_cast<int>(e.idx_ts.us.count())));
      }
      EXPECT_EQ("foo 0 100,bar 1 200,meh 2 500,blah 3 999", Join(all_four, ","));
      std::vector<std::string> all_four_unsafe;
      for (const auto& e : impl.IterateUnsafe()) {
        all_four_unsafe.push_back(e);
      }
      EXPECT_EQ(
          "{\"index\":0,\"us\":100}\t{\"s\":\"foo\"},"
          "{\"index\":1,\"us\":200}\t{\"s\":\"bar\"},"
          "{\"index\":2,\"us\":500}\t{\"s\":\"meh\"},"
          "{\"index\":3,\"us\":999}\t{\"s\":\"blah\"}",
          Join(all_four_unsafe, ","));
    }
  }

  {
    // Confirm the added, fourth, entry, has been appended properly with respect to replaying the file.
    std::mutex mutex;
    IMPL impl(mutex, namespace_name, persistence_file_name);
    EXPECT_EQ(4u, impl.Size());

    std::vector<std::string> all_four;
    for (const auto& e : impl.Iterate()) {
      all_four.push_back(Printf(
          "%s %d %d", e.entry.s.c_str(), static_cast<int>(e.idx_ts.index), static_cast<int>(e.idx_ts.us.count())));
    }
    EXPECT_EQ("foo 0 100,bar 1 200,meh 2 500,blah 3 999", Join(all_four, ","));
    std::vector<std::string> all_four_unsafe;
    for (const auto& e : impl.IterateUnsafe()) {
      all_four_unsafe.push_back(e);
    }
    EXPECT_EQ(
        "{\"index\":0,\"us\":100}\t{\"s\":\"foo\"},"
        "{\"index\":1,\"us\":200}\t{\"s\":\"bar\"},"
        "{\"index\":2,\"us\":500}\t{\"s\":\"meh\"},"
        "{\"index\":3,\"us\":999}\t{\"s\":\"blah\"}",
        Join(all_four_unsafe, ","));
  }
}

TEST(PersistenceLayer, FileDirectives) {
  using namespace persistence_test;

  using IMPL = current::persistence::File<StorableString>;

  const auto namespace_name = current::ss::StreamNamespaceName("namespace", "entry_name");
  const std::string persistence_file_name = current::FileSystem::JoinPath(FLAGS_persistence_test_tmpdir, "data");
  const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

  {
    // An empty file - no entries and head equals -1us.
    std::mutex mutex;
    IMPL impl(mutex, namespace_name, persistence_file_name);
    EXPECT_EQ(0u, impl.Size());
    EXPECT_EQ(-1, impl.CurrentHead().count());
    const auto head_idxts = impl.HeadAndLastPublishedIndexAndTimestamp();
    ASSERT_FALSE(Exists(head_idxts.idxts));
    EXPECT_EQ(-1, head_idxts.head.count());
  }
  current::reflection::StructSchema struct_schema;
  struct_schema.AddType<StorableString>();
  const std::string signature =
      "#signature " + JSON(current::ss::StreamSignature(namespace_name, struct_schema.GetSchemaInfo())) + '\n';
  EXPECT_EQ(signature, current::FileSystem::ReadFileAsString(persistence_file_name));

  {
    current::time::ResetToZero();

    // A file consisting only of directives.
    current::FileSystem::WriteStringToFile(signature +
                                               "#head\t0000000000000000001\n"
                                               "#unknown_directive\t\tblah\n",
                                           persistence_file_name.c_str());
    // Skip unknown directives.
    std::mutex mutex;
    IMPL impl(mutex, namespace_name, persistence_file_name);
    EXPECT_EQ(1, impl.CurrentHead().count());
    // Append a new head directive, because after the last one there was another directive.
    current::time::SetNow(std::chrono::microseconds(2));
    impl.UpdateHead();
    EXPECT_EQ(2, impl.CurrentHead().count());
    const auto head_idxts = impl.HeadAndLastPublishedIndexAndTimestamp();
    ASSERT_FALSE(Exists(head_idxts.idxts));
    EXPECT_EQ(2, head_idxts.head.count());
  }
  EXPECT_EQ(signature +
                "#head\t0000000000000000001\n"
                "#unknown_directive\t\tblah\n"
                "#head 00000000000000000002\n",
            current::FileSystem::ReadFileAsString(persistence_file_name));

  {
    current::time::ResetToZero();

    current::FileSystem::WriteStringToFile(
        "{\"index\":0,\"us\":100}\t{\"s\":\"foo\"}\n"
        "{\"index\":1,\"us\":200}\t{\"s\":\"bar\"}\n"
        "#head\t00000000000000000300\n"
        "#some_other_directive\n"
        "#head  00000000000000000400\n"
        "{\"index\":2,\"us\":500}\t{\"s\":\"meh\"}\n"
        "#head\t \t00000000000000000600\n",
        persistence_file_name.c_str());
    // Several head directives with different key-value delimeters.
    std::mutex mutex;
    IMPL impl(mutex, namespace_name, persistence_file_name);
    EXPECT_EQ(3u, impl.Size());
    EXPECT_EQ(600, impl.CurrentHead().count());
    auto head_idxts = impl.HeadAndLastPublishedIndexAndTimestamp();
    ASSERT_TRUE(Exists(head_idxts.idxts));
    EXPECT_EQ(2u, Value(head_idxts.idxts).index);
    EXPECT_EQ(500, Value(head_idxts.idxts).us.count());
    EXPECT_EQ(600, head_idxts.head.count());

    // Rewrite the last head directive.
    current::time::SetNow(std::chrono::microseconds(700));
    impl.UpdateHead();
    EXPECT_EQ(700, impl.CurrentHead().count());
    current::time::SetNow(std::chrono::microseconds(800));

    impl.Publish(StorableString("new"));
    EXPECT_EQ(800, impl.CurrentHead().count());
    // Append a new head directive, because there was an entry after the last one.
    current::time::SetNow(std::chrono::microseconds(999));
    impl.UpdateHead();
    EXPECT_EQ(4u, impl.Size());
    EXPECT_EQ(999, impl.CurrentHead().count());
    head_idxts = impl.HeadAndLastPublishedIndexAndTimestamp();
    ASSERT_TRUE(Exists(head_idxts.idxts));
    EXPECT_EQ(3u, Value(head_idxts.idxts).index);
    EXPECT_EQ(800, Value(head_idxts.idxts).us.count());
    EXPECT_EQ(999, head_idxts.head.count());
  }
  EXPECT_EQ(
      "{\"index\":0,\"us\":100}\t{\"s\":\"foo\"}\n"
      "{\"index\":1,\"us\":200}\t{\"s\":\"bar\"}\n"
      "#head\t00000000000000000300\n"
      "#some_other_directive\n"
      "#head  00000000000000000400\n"
      "{\"index\":2,\"us\":500}\t{\"s\":\"meh\"}\n"
      "#head\t \t00000000000000000700\n"
      "{\"index\":3,\"us\":800}\t{\"s\":\"new\"}\n"
      "#head 00000000000000000999\n",
      current::FileSystem::ReadFileAsString(persistence_file_name));
}

TEST(PersistenceLayer, FileExceptions) {
  using namespace persistence_test;

  using IMPL = current::persistence::File<std::string>;

  static_assert(current::ss::IsPersister<IMPL>::value, "");
  static_assert(current::ss::IsEntryPersister<IMPL, std::string>::value, "");

  static_assert(!current::ss::IsPublisher<IMPL>::value, "");
  static_assert(!current::ss::IsEntryPublisher<IMPL, std::string>::value, "");

  static_assert(!current::ss::IsPublisher<int>::value, "");
  static_assert(!current::ss::IsEntryPublisher<IMPL, int>::value, "");

  const auto namespace_name = current::ss::StreamNamespaceName("namespace", "entry_name");
  const std::string persistence_file_name = current::FileSystem::JoinPath(FLAGS_persistence_test_tmpdir, "data");

  {
    current::time::ResetToZero();
    const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);
    // Time goes back.
    std::mutex mutex;
    IMPL impl(mutex, namespace_name, persistence_file_name);
    current::time::SetNow(std::chrono::microseconds(2));
    impl.Publish("2");
    current::time::ResetToZero();
    current::time::SetNow(std::chrono::microseconds(1));
    ASSERT_THROW(impl.Publish("1"), current::ss::InconsistentTimestampException);
    ASSERT_THROW(impl.UpdateHead(), current::ss::InconsistentTimestampException);
    impl.UpdateHead(std::chrono::microseconds(4));
    current::time::SetNow(std::chrono::microseconds(3));
    ASSERT_THROW(impl.Publish("1"), current::ss::InconsistentTimestampException);
    ASSERT_THROW(impl.UpdateHead(), current::ss::InconsistentTimestampException);
  }

  {
    current::time::ResetToZero();
    const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);
    // Time staying the same is as bad as time going back.
    current::time::SetNow(std::chrono::microseconds(3));
    std::mutex mutex;
    IMPL impl(mutex, namespace_name, persistence_file_name);
    impl.Publish("2");
    ASSERT_THROW(impl.Publish("1"), current::ss::InconsistentTimestampException);
    ASSERT_THROW(impl.UpdateHead(), current::ss::InconsistentTimestampException);
    current::time::SetNow(std::chrono::microseconds(4));
    impl.UpdateHead();
    ASSERT_THROW(impl.Publish("1"), current::ss::InconsistentTimestampException);
    ASSERT_THROW(impl.UpdateHead(), current::ss::InconsistentTimestampException);
  }

  {
    current::time::ResetToZero();
    const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);
    std::mutex mutex;
    IMPL impl(mutex, namespace_name, persistence_file_name);
    ASSERT_THROW(impl.LastPublishedIndexAndTimestamp(), current::persistence::NoEntriesPublishedYet);
  }

  {
    current::time::ResetToZero();
    const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);
    std::mutex mutex;
    IMPL impl(mutex, namespace_name, persistence_file_name);
    current::time::SetNow(std::chrono::microseconds(1));
    impl.Publish("1");
    current::time::SetNow(std::chrono::microseconds(2));
    impl.Publish("2");
    current::time::SetNow(std::chrono::microseconds(3));
    impl.Publish("3");
    ASSERT_THROW(impl.Iterate(1, 0), current::persistence::InvalidIterableRangeException);
    ASSERT_THROW((impl.IterateUnsafe(1, 0)), current::persistence::InvalidIterableRangeException);
    ASSERT_THROW(impl.Iterate(100, 101), current::persistence::InvalidIterableRangeException);
    ASSERT_THROW((impl.IterateUnsafe(100, 101)), current::persistence::InvalidIterableRangeException);
    ASSERT_THROW(impl.Iterate(100, 100), current::persistence::InvalidIterableRangeException);
    ASSERT_THROW((impl.IterateUnsafe(100, 100)), current::persistence::InvalidIterableRangeException);
  }
}

TEST(PersistenceLayer, FileSignatureExceptions) {
  using namespace persistence_test;

  using IMPL = current::persistence::File<std::string>;

  const auto namespace_name = current::ss::StreamNamespaceName("namespace", "entry_name");
  const std::string persistence_file_name = current::FileSystem::JoinPath(FLAGS_persistence_test_tmpdir, "data");
  current::reflection::StructSchema invalid_schema;
  invalid_schema.AddType<std::string>();
  const std::string signature =
      "#signature " + JSON(current::ss::StreamSignature(namespace_name, invalid_schema.GetSchemaInfo())) + '\n';

  {
    // Invalid signature.
    const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);
    using INVALID_IMPL = current::persistence::File<StorableString>;
    const auto another_namespace = current::ss::StreamNamespaceName("namespace_invalid", "top_level_invalid");
    current::FileSystem::WriteStringToFile(signature + "{\"index\":0,\"us\":1}\t{\"s\":\"foo\"}\n",
                                           persistence_file_name.c_str());
    std::mutex mutex;
    ASSERT_NO_THROW(IMPL(mutex, namespace_name, persistence_file_name));
    ASSERT_THROW(IMPL(mutex, another_namespace, persistence_file_name), current::persistence::InvalidStreamSignature);
    ASSERT_THROW(INVALID_IMPL(mutex, namespace_name, persistence_file_name),
                 current::persistence::InvalidStreamSignature);
  }

  {
    // Signature in the middle of the data file, not at the top.
    const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);
    current::FileSystem::WriteStringToFile("{\"index\":0,\"us\":1}\t{\"s\":\"foo\"}\n" + signature,
                                           persistence_file_name.c_str());
    std::mutex mutex;
    ASSERT_THROW(IMPL(mutex, namespace_name, persistence_file_name), current::persistence::InvalidSignatureLocation);
    current::FileSystem::WriteStringToFile("#any_other_directive\n" + signature, persistence_file_name.c_str());
    ASSERT_THROW(IMPL(mutex, namespace_name, persistence_file_name), current::persistence::InvalidSignatureLocation);
    current::FileSystem::WriteStringToFile(signature + signature, persistence_file_name.c_str());
    ASSERT_THROW(IMPL(mutex, namespace_name, persistence_file_name), current::persistence::InvalidSignatureLocation);
  }
}

TEST(PersistenceLayer, FileSafeVsUnsafeIterators) {
  using namespace persistence_test;

  using IMPL = current::persistence::File<StorableString>;

  const auto namespace_name = current::ss::StreamNamespaceName("namespace", "entry_name");
  const std::string persistence_file_name = current::FileSystem::JoinPath(FLAGS_persistence_test_tmpdir, "data");
  const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

  current::reflection::StructSchema struct_schema;
  struct_schema.AddType<StorableString>();
  const std::string signature =
      "#signature " + JSON(current::ss::StreamSignature(namespace_name, struct_schema.GetSchemaInfo())) + '\n';
  std::vector<std::string> lines = {"{\"index\":0,\"us\":100}\t{\"s\":\"foo\"}",
                                    "{\"index\":1,\"us\":200}\t{\"s\":\"bar\"}",
                                    "#head 00000000000000000300",
                                    "{\"index\":2,\"us\":500}\t{\"s\":\"meh\"}",
                                    "#head 00000000000000000600"};
  const auto CombineFileContents = [&]() -> std::string {
    std::string contents = signature;
    for (const auto& line : lines) {
      contents += line + '\n';
    }
    return contents;
  };

  current::FileSystem::WriteStringToFile(CombineFileContents(), persistence_file_name.c_str());

  std::mutex mutex;
  IMPL impl(mutex, namespace_name, persistence_file_name);

  const auto GetUnsafeIterationResult = [&]() -> std::string {
    std::string combined_result;
    for (const auto& e : impl.IterateUnsafe()) {
      combined_result += e + '\n';
    }
    return combined_result;
  };

  {
    std::vector<std::string> all_entries;
    for (const auto& e : impl.Iterate()) {
      all_entries.push_back(Printf(
          "%s %d %d", e.entry.s.c_str(), static_cast<int>(e.idx_ts.index), static_cast<int>(e.idx_ts.us.count())));
    }
    EXPECT_EQ("foo 0 100,bar 1 200,meh 2 500", Join(all_entries, ","));
    EXPECT_EQ(
        "{\"index\":0,\"us\":100}\t{\"s\":\"foo\"}\n"
        "{\"index\":1,\"us\":200}\t{\"s\":\"bar\"}\n"
        "{\"index\":2,\"us\":500}\t{\"s\":\"meh\"}\n",
        GetUnsafeIterationResult());
  }

  {
    EXPECT_NO_THROW(*impl.Iterate(0, 1).begin());
    EXPECT_NO_THROW(*impl.Iterate(1, 2).begin());
    EXPECT_NO_THROW(*impl.Iterate().begin());
    EXPECT_NO_THROW(*(++impl.Iterate().begin()));

    // Swap the first two entries to provoke the `InconsistentIndexException` exceptions.
    std::swap(lines[0], lines[1]);
    current::FileSystem::WriteStringToFile(CombineFileContents(), persistence_file_name.c_str());

    // Check that "safe" iterators throw the InconsistentIndexException while "unsafe" ones don't.
    EXPECT_THROW(*impl.Iterate(0, 1).begin(), current::ss::InconsistentIndexException);
    EXPECT_THROW(*impl.Iterate(1, 2).begin(), current::ss::InconsistentIndexException);
    EXPECT_THROW(*impl.Iterate().begin(), current::ss::InconsistentIndexException);
    EXPECT_THROW(*(++impl.Iterate().begin()), current::ss::InconsistentIndexException);
    EXPECT_NO_THROW((*impl.IterateUnsafe(0, 1).begin()));
    EXPECT_NO_THROW((*impl.IterateUnsafe(1, 2).begin()));
    EXPECT_NO_THROW((*impl.IterateUnsafe().begin()));
    EXPECT_NO_THROW((*(++impl.IterateUnsafe().begin())));
    EXPECT_EQ(
        "{\"index\":1,\"us\":200}\t{\"s\":\"bar\"}\n"
        "{\"index\":0,\"us\":100}\t{\"s\":\"foo\"}\n"
        "{\"index\":2,\"us\":500}\t{\"s\":\"meh\"}\n",
        GetUnsafeIterationResult());
    std::swap(lines[0], lines[1]);
  }

  {
    // Produce wrong JSON instead of an entire entry - replace both index and value with garbage.
    lines[1].replace(0, lines[1].length(), lines[1].length(), 'A');
    current::FileSystem::WriteStringToFile(CombineFileContents(), persistence_file_name.c_str());

    // Check that "safe" iterators throw the MalformedEntryException while "unsafe" ones don't.
    EXPECT_THROW(*(++(impl.Iterate().begin())), current::persistence::MalformedEntryException);
    EXPECT_THROW(*impl.Iterate(1, 2).begin(), current::persistence::MalformedEntryException);
    EXPECT_NO_THROW((*impl.IterateUnsafe(1, 2).begin()));
    EXPECT_NO_THROW((*(++impl.IterateUnsafe().begin())));
    EXPECT_EQ(
        "{\"index\":0,\"us\":100}\t{\"s\":\"foo\"}\n"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
        "{\"index\":2,\"us\":500}\t{\"s\":\"meh\"}\n",
        GetUnsafeIterationResult());
  }

  {
    EXPECT_NO_THROW(*impl.Iterate(0, 1).begin());
    EXPECT_NO_THROW(*impl.Iterate().begin());

    // Produce wrong JSON instead of the index and timestamp only and leave the entry value valid.
    const auto tab_pos = lines[0].find('\t');
    ASSERT_FALSE(std::string::npos == tab_pos);
    lines[0].replace(0, tab_pos, tab_pos, 'B');
    current::FileSystem::WriteStringToFile(CombineFileContents(), persistence_file_name.c_str());

    // Check that "safe" iterators throw the InvalidJSONException while "unsafe" ones don't.
    EXPECT_THROW(*impl.Iterate(0, 1).begin(), current::serialization::json::InvalidJSONException);
    EXPECT_THROW(*impl.Iterate().begin(), current::serialization::json::InvalidJSONException);
    EXPECT_NO_THROW((*impl.IterateUnsafe(0, 1).begin()));
    EXPECT_EQ(
        "BBBBBBBBBBBBBBBBBBBB\t{\"s\":\"foo\"}\n"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
        "{\"index\":2,\"us\":500}\t{\"s\":\"meh\"}\n",
        GetUnsafeIterationResult());
  }

  {
    EXPECT_NO_THROW(*impl.Iterate(2, 3).begin());

    // Produce wrong JSON instead of the entry's value only and leave index and timestamp section valid.
    const auto tab_pos = lines[3].find('\t');
    ASSERT_FALSE(std::string::npos == tab_pos);
    const auto length_to_replace = lines[3].length() - (tab_pos + 1);
    lines[3].replace(tab_pos + 1, length_to_replace, length_to_replace, 'C');
    current::FileSystem::WriteStringToFile(CombineFileContents(), persistence_file_name.c_str());

    // Check that "safe" iterators throw the InvalidJSONException while "unsafe" ones don't.
    EXPECT_THROW(*impl.Iterate(2, 3).begin(), current::serialization::json::InvalidJSONException);
    EXPECT_NO_THROW((*impl.IterateUnsafe(2, 3).begin()));
    EXPECT_EQ(
        "BBBBBBBBBBBBBBBBBBBB\t{\"s\":\"foo\"}\n"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
        "{\"index\":2,\"us\":500}\tCCCCCCCCCCC\n",
        GetUnsafeIterationResult());
  }
}

namespace persistence_test {

inline StorableString LargeTestStorableString(int index) {
  return StorableString{Printf("%07d ", index) + std::string(3 + index % 7, 'a' + index % 26)};
}

template <typename IMPL, int N = 1000>
void IteratorPerformanceTest(IMPL& impl, bool publish = true) {
  current::time::ResetToZero();
  using us_t = std::chrono::microseconds;

  // Populate many entries. Skip if testing the "resume from an existing file" mode.
  if (publish) {
    EXPECT_EQ(0u, impl.Size());
    for (int i = 0; i < N; ++i) {
      current::time::SetNow(std::chrono::microseconds(i * 1000));
      impl.Publish(LargeTestStorableString(i));
    }
  }
  EXPECT_EQ(static_cast<size_t>(N), impl.Size());

  // Confirm entries are as expected.
  {
    // By index.
    EXPECT_EQ(0ull, (*impl.Iterate(0, 1).begin()).idx_ts.index);
    EXPECT_EQ(0ll, (*impl.Iterate(0, 1).begin()).idx_ts.us.count());
    EXPECT_EQ("0000000 aaa", (*impl.Iterate(0, 1).begin()).entry.s);
    EXPECT_EQ("{\"index\":0,\"us\":0}\t{\"s\":\"0000000 aaa\"}", *(impl.IterateUnsafe(0, 1).begin()));
    EXPECT_EQ(10ull, (*impl.Iterate(10, 11).begin()).idx_ts.index);
    EXPECT_EQ(10000ll, (*impl.Iterate(10, 11).begin()).idx_ts.us.count());
    EXPECT_EQ("0000010 kkkkkk", (*impl.Iterate(10, 11).begin()).entry.s);
    EXPECT_EQ("{\"index\":10,\"us\":10000}\t{\"s\":\"0000010 kkkkkk\"}",
              (*impl.IterateUnsafe(10, 11).begin()));
    EXPECT_EQ(100ull, (*impl.Iterate(100, 101).begin()).idx_ts.index);
    EXPECT_EQ(100000ll, (*impl.Iterate(100, 101).begin()).idx_ts.us.count());
    EXPECT_EQ("0000100 wwwww", (*impl.Iterate(100, 101).begin()).entry.s);
    EXPECT_EQ("{\"index\":100,\"us\":100000}\t{\"s\":\"0000100 wwwww\"}",
              (*impl.IterateUnsafe(100, 101).begin()));
  }
  {
    // By timestamp.
    EXPECT_EQ(0ull, (*impl.Iterate(us_t(0), us_t(1000)).begin()).idx_ts.index);
    EXPECT_EQ(0ll, (*impl.Iterate(us_t(0), us_t(1000)).begin()).idx_ts.us.count());
    EXPECT_EQ("0000000 aaa", (*impl.Iterate(us_t(0), us_t(1000)).begin()).entry.s);
    EXPECT_EQ("{\"index\":0,\"us\":0}\t{\"s\":\"0000000 aaa\"}",
              (*impl.IterateUnsafe(us_t(0), us_t(1000)).begin()));
    EXPECT_EQ(10ull, (*impl.Iterate(us_t(10000), us_t(11000)).begin()).idx_ts.index);
    EXPECT_EQ(10000ll, (*impl.Iterate(us_t(10000), us_t(11000)).begin()).idx_ts.us.count());
    EXPECT_EQ("0000010 kkkkkk", (*impl.Iterate(us_t(10000), us_t(11000)).begin()).entry.s);
    EXPECT_EQ("{\"index\":10,\"us\":10000}\t{\"s\":\"0000010 kkkkkk\"}",
              (*impl.IterateUnsafe(us_t(10000), us_t(11000)).begin()));
    EXPECT_EQ(100ull, (*impl.Iterate(us_t(100000), us_t(101000)).begin()).idx_ts.index);
    EXPECT_EQ(100000ll, (*impl.Iterate(us_t(100000), us_t(101000)).begin()).idx_ts.us.count());
    EXPECT_EQ("0000100 wwwww", (*impl.Iterate(us_t(100000), us_t(101000)).begin()).entry.s);
    EXPECT_EQ("{\"index\":100,\"us\":100000}\t{\"s\":\"0000100 wwwww\"}",
              (*impl.IterateUnsafe(us_t(100000), us_t(101000)).begin()));
  }

  // Perftest the creation of a large number of iterators.
  // The test would pass swiftly if the file is being seeked to the right spot,
  // and run forever if every new iteator is scanning the file from the very beginning.
  for (int i = 0; i < N; ++i) {
    const auto cit = impl.Iterate(i, i + 1).begin();
    const auto& e = *cit;
    const auto cit_unsafe = impl.IterateUnsafe(i, i + 1).begin();
    const auto& e_unsafe = *cit_unsafe;
    EXPECT_EQ(JSON(e.idx_ts), JSON((*impl.Iterate(us_t(i * 1000), us_t((i + 1) * 1000)).begin()).idx_ts));
    EXPECT_EQ(static_cast<uint64_t>(i), e.idx_ts.index);
    EXPECT_EQ(static_cast<int64_t>(i * 1000), e.idx_ts.us.count());
    EXPECT_EQ(LargeTestStorableString(i).s, e.entry.s);
    EXPECT_EQ(JSON((*impl.Iterate(us_t(i * 1000), us_t((i + 1) * 1000)).begin()).idx_ts) + '\t' +
                  JSON(LargeTestStorableString(i)),
              e_unsafe);
  }
}

}  // namespace persistence_test

TEST(PersistenceLayer, MemoryIteratorPerformanceTest) {
  using namespace persistence_test;
  using IMPL = current::persistence::Memory<StorableString>;
  std::mutex mutex;
  const auto namespace_name = current::ss::StreamNamespaceName("namespace", "entry_name");
  IMPL impl(mutex, namespace_name);
  IteratorPerformanceTest(impl);
}

TEST(PersistenceLayer, FileIteratorPerformanceTest) {
  using namespace persistence_test;
  using IMPL = current::persistence::File<StorableString>;
  const auto namespace_name = current::ss::StreamNamespaceName("namespace", "entry_name");
  const std::string persistence_file_name = current::FileSystem::JoinPath(FLAGS_persistence_test_tmpdir, "data");
  const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);
  {
    // First, run the proper test.
    std::mutex mutex;
    IMPL impl(mutex, namespace_name, persistence_file_name);
    IteratorPerformanceTest(impl);
  }
  {
    // Then, test file resume logic as well.
    std::mutex mutex;
    IMPL impl(mutex, namespace_name, persistence_file_name);
    IteratorPerformanceTest(impl, false);
  }
}

TEST(PersistenceLayer, FileIteratorCanNotOutliveFile) {
  using namespace persistence_test;
  using IMPL = current::persistence::File<std::string>;
  std::mutex mutex;
  const auto namespace_name = current::ss::StreamNamespaceName("namespace", "entry_name");
  const std::string persistence_file_name = current::FileSystem::JoinPath(FLAGS_persistence_test_tmpdir, "data");
  const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

  auto p = std::make_unique<IMPL>(mutex, namespace_name, persistence_file_name);
  p->Publish("1", std::chrono::microseconds(1));
  p->Publish("2", std::chrono::microseconds(2));
  p->Publish("3", std::chrono::microseconds(3));

  std::thread t;  // To wait for the persister to shut down as iterators over it are done.

  {
    auto iterable = p->Iterate();
    auto iterable_unsafe = p->IterateUnsafe();
    EXPECT_TRUE(static_cast<bool>(iterable));
    EXPECT_TRUE(static_cast<bool>(iterable_unsafe));
    auto iterator = iterable.begin();
    auto iterator_unsafe = iterable_unsafe.begin();
    EXPECT_TRUE(static_cast<bool>(iterator));
    EXPECT_TRUE(static_cast<bool>(iterator_unsafe));
    EXPECT_EQ("1", (*iterator).entry);
    EXPECT_EQ("{\"index\":0,\"us\":1}\t\"1\"", *iterator_unsafe);

    t = std::thread([&p]() {
      // Release the persister. Well, begin to, as this "call" would block until the iterators are done.
      p = nullptr;
    });

    // Spin lock, and w/o a mutex it would hang with `NDEBUG=1`.
    {
      std::mutex aux_mutex;
      while (true) {
        std::lock_guard<std::mutex> lock(aux_mutex);
        if (!iterator && !iterator_unsafe) {
          break;
        }
        std::this_thread::yield();
      }
    }

    ASSERT_FALSE(static_cast<bool>(iterator));         // Should no longer be available.
    ASSERT_FALSE(static_cast<bool>(iterator_unsafe));  // Should no longer be available.

    // Spin lock, and w/o a mutex it would hang with `NDEBUG=1`.
    {
      std::mutex aux_mutex;
      while (true) {
        std::lock_guard<std::mutex> lock(aux_mutex);
        if (!iterable && !iterable_unsafe) {
          break;
        }
        std::this_thread::yield();
      }
    }

    ASSERT_FALSE(static_cast<bool>(iterable));         // Should no longer be available.
    ASSERT_FALSE(static_cast<bool>(iterable_unsafe));  // Should no longer be available.
  }

  t.join();
}

TEST(PersistenceLayer, Exceptions) {
  using namespace persistence_test;
  using IMPL = current::persistence::File<StorableString>;
  using current::ss::IndexAndTimestamp;
  using current::ss::InconsistentTimestampException;
  using current::ss::InconsistentIndexException;
  using current::persistence::MalformedEntryException;

  const auto namespace_name = current::ss::StreamNamespaceName("namespace", "entry_name");
  const std::string persistence_file_name = current::FileSystem::JoinPath(FLAGS_persistence_test_tmpdir, "data");

  // Malformed entry during replay.
  {
    const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);
    current::FileSystem::WriteStringToFile("Malformed entry", persistence_file_name.c_str());
    std::mutex mutex;
    EXPECT_THROW(IMPL impl(mutex, namespace_name, persistence_file_name), MalformedEntryException);
  }
  // Inconsistent index during replay.
  {
    const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);
    current::FileSystem::WriteStringToFile(
        "{\"index\":0,\"us\":100}\t{\"s\":\"foo\"}\n"
        "{\"index\":0,\"us\":200}\t{\"s\":\"bar\"}\n",
        persistence_file_name.c_str());
    std::mutex mutex;
    EXPECT_THROW(IMPL impl(mutex, namespace_name, persistence_file_name), InconsistentIndexException);
  }
  // Inconsistent timestamp during replay.
  {
    const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);
    current::FileSystem::WriteStringToFile(
        "{\"index\":0,\"us\":150}\t{\"s\":\"foo\"}\n"
        "{\"index\":1,\"us\":150}\t{\"s\":\"bar\"}\n",
        persistence_file_name.c_str());
    std::mutex mutex;
    EXPECT_THROW(IMPL impl(mutex, namespace_name, persistence_file_name), InconsistentTimestampException);
  }
}
