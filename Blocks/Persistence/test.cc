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

#include "persistence.h"

#include "../SS/ss.h"

#include "../../Bricks/dflags/dflags.h"
#include "../../Bricks/file/file.h"
#include "../../Bricks/strings/join.h"
#include "../../Bricks/strings/printf.h"

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

  {
    IMPL impl;
    EXPECT_EQ(0u, impl.Size());

    current::time::SetNow(std::chrono::microseconds(100));
    impl.Publish("foo");
    current::time::SetNow(std::chrono::microseconds(200));
    impl.Publish("bar");
    current::time::SetNow(std::chrono::microseconds(300));
    EXPECT_EQ(2u, impl.Size());

    {
      std::vector<std::string> first_two;
      for (const auto& e : impl.Iterate()) {
        first_two.push_back(Printf("%s %d %d",
                                   e.entry.c_str(),
                                   static_cast<int>(e.idx_ts.index),
                                   static_cast<int>(e.idx_ts.us.count())));
      }
      EXPECT_EQ("foo 0 100,bar 1 200", Join(first_two, ","));
    }

    impl.Publish("meh");
    EXPECT_EQ(3u, impl.Size());

    {
      std::vector<std::string> all_three;
      for (const auto& e : impl.Iterate()) {
        all_three.push_back(Printf("%s %d %d",
                                   e.entry.c_str(),
                                   static_cast<int>(e.idx_ts.index),
                                   static_cast<int>(e.idx_ts.us.count())));
      }
      EXPECT_EQ("foo 0 100,bar 1 200,meh 2 300", Join(all_three, ","));
    }

    {
      std::vector<std::string> just_the_last_one;
      for (const auto& e : impl.Iterate(2)) {
        just_the_last_one.push_back(e.entry);
      }
      EXPECT_EQ("meh", Join(just_the_last_one, ","));
    }
  }

  {
    // Obviously, no state is shared for `Memory` implementation.
    // The data starts from ground zero.
    IMPL impl;
    EXPECT_EQ(0u, impl.Size());
  }
}

TEST(PersistenceLayer, File) {
  using namespace persistence_test;

  using IMPL = current::persistence::File<StorableString>;

  static_assert(current::ss::IsPersister<IMPL>::value, "");
  static_assert(current::ss::IsEntryPersister<IMPL, StorableString>::value, "");

  static_assert(!current::ss::IsPublisher<IMPL>::value, "");
  static_assert(!current::ss::IsEntryPublisher<IMPL, StorableString>::value, "");

  static_assert(!current::ss::IsPublisher<int>::value, "");
  static_assert(!current::ss::IsEntryPublisher<IMPL, int>::value, "");

  const std::string persistence_file_name =
      current::FileSystem::JoinPath(FLAGS_persistence_test_tmpdir, "data");
  const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

  {
    IMPL impl(persistence_file_name);
    EXPECT_EQ(0u, impl.Size());
    current::time::SetNow(std::chrono::microseconds(100));
    impl.Publish(StorableString("foo"));
    current::time::SetNow(std::chrono::microseconds(200));
    impl.Publish(std::move(StorableString("bar")));
    EXPECT_EQ(2u, impl.Size());

    {
      std::vector<std::string> first_two;
      for (const auto& e : impl.Iterate()) {
        first_two.push_back(Printf("%s %d %d",
                                   e.entry.s.c_str(),
                                   static_cast<int>(e.idx_ts.index),
                                   static_cast<int>(e.idx_ts.us.count())));
      }
      EXPECT_EQ("foo 0 100,bar 1 200", Join(first_two, ","));
    }

    current::time::SetNow(std::chrono::microseconds(500));
    impl.Publish(StorableString("meh"));
    EXPECT_EQ(3u, impl.Size());

    {
      std::vector<std::string> all_three;
      for (const auto& e : impl.Iterate()) {
        all_three.push_back(Printf("%s %d %d",
                                   e.entry.s.c_str(),
                                   static_cast<int>(e.idx_ts.index),
                                   static_cast<int>(e.idx_ts.us.count())));
      }
      EXPECT_EQ("foo 0 100,bar 1 200,meh 2 500", Join(all_three, ","));
    }
  }

  EXPECT_EQ(
      "{\"index\":0,\"us\":100}\t{\"s\":\"foo\"}\n"
      "{\"index\":1,\"us\":200}\t{\"s\":\"bar\"}\n"
      "{\"index\":2,\"us\":500}\t{\"s\":\"meh\"}\n",
      current::FileSystem::ReadFileAsString(persistence_file_name));

  {
    // Confirm the data has been saved and can be replayed.
    IMPL impl(persistence_file_name);
    EXPECT_EQ(3u, impl.Size());

    {
      std::vector<std::string> all_three;
      for (const auto& e : impl.Iterate()) {
        all_three.push_back(Printf("%s %d %d",
                                   e.entry.s.c_str(),
                                   static_cast<int>(e.idx_ts.index),
                                   static_cast<int>(e.idx_ts.us.count())));
      }
      EXPECT_EQ("foo 0 100,bar 1 200,meh 2 500", Join(all_three, ","));
    }

    current::time::SetNow(std::chrono::microseconds(999));
    impl.Publish(StorableString("blah"));
    EXPECT_EQ(4u, impl.Size());

    {
      std::vector<std::string> all_four;
      for (const auto& e : impl.Iterate()) {
        all_four.push_back(Printf("%s %d %d",
                                  e.entry.s.c_str(),
                                  static_cast<int>(e.idx_ts.index),
                                  static_cast<int>(e.idx_ts.us.count())));
      }
      EXPECT_EQ("foo 0 100,bar 1 200,meh 2 500,blah 3 999", Join(all_four, ","));
    }
  }

  {
    // Confirm the added, fourth, entry, has been appended properly with respect to replaying the file.
    IMPL impl(persistence_file_name);
    EXPECT_EQ(4u, impl.Size());

    std::vector<std::string> all_four;
    for (const auto& e : impl.Iterate()) {
      all_four.push_back(Printf("%s %d %d",
                                e.entry.s.c_str(),
                                static_cast<int>(e.idx_ts.index),
                                static_cast<int>(e.idx_ts.us.count())));
    }
    EXPECT_EQ("foo 0 100,bar 1 200,meh 2 500,blah 3 999", Join(all_four, ","));
  }
}

TEST(PersistenceLayer, Exceptions) {
  using namespace persistence_test;
  using IMPL = current::persistence::File<StorableString>;
  using current::ss::IndexAndTimestamp;
  using current::persistence::MalformedEntryException;
  using current::persistence::InconsistentIndexException;
  using current::persistence::InconsistentTimestampException;

  const std::string persistence_file_name =
      current::FileSystem::JoinPath(FLAGS_persistence_test_tmpdir, "data");

  // Malformed entry during replay.
  {
    const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);
    current::FileSystem::WriteStringToFile("Malformed entry", persistence_file_name.c_str());
    EXPECT_THROW(IMPL impl(persistence_file_name), MalformedEntryException);
  }
  // Inconsistent index during replay.
  {
    const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);
    current::FileSystem::WriteStringToFile(
        "{\"index\":0,\"us\":100}\t{\"s\":\"foo\"}\n"
        "{\"index\":0,\"us\":200}\t{\"s\":\"bar\"}\n",
        persistence_file_name.c_str());
    EXPECT_THROW(IMPL impl(persistence_file_name), InconsistentIndexException);
  }
  // Inconsistent timestamp during replay.
  {
    const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);
    current::FileSystem::WriteStringToFile(
        "{\"index\":0,\"us\":150}\t{\"s\":\"foo\"}\n"
        "{\"index\":1,\"us\":150}\t{\"s\":\"bar\"}\n",
        persistence_file_name.c_str());
    EXPECT_THROW(IMPL impl(persistence_file_name), InconsistentTimestampException);
  }
}
