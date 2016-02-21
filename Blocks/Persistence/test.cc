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

#include <atomic>
#include <string>

#define CURRENT_MOCK_TIME  // `SetNow()`.

#include "persistence.h"

#include "../../Bricks/dflags/dflags.h"
#include "../../Bricks/file/file.h"
#include "../../Bricks/strings/join.h"

#include "../../Bricks/util/clone.h"

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_string(persistence_test_tmpdir, ".current", "Local path for the test to create temporary files in.");

namespace persistence_test {
using current::strings::Join;

CURRENT_STRUCT(StorableString) {
  CURRENT_FIELD(s, std::string, "");
  CURRENT_DEFAULT_CONSTRUCTOR(StorableString) {}
  CURRENT_CONSTRUCTOR(StorableString)(const std::string& s) : s(s) {}
};

using IDX_TS = current::ss::IndexAndTimestamp;
using current::ss::EntryResponse;
using current::ss::TerminationResponse;

struct PersistenceTestListenerImpl {
  std::atomic_size_t seen;
  std::atomic_bool end_reached;

  std::vector<std::string> messages;

  PersistenceTestListenerImpl() : seen(0u), end_reached(false) {}

  EntryResponse operator()(const std::string& message, IDX_TS current, IDX_TS last) {
    messages.push_back(message);
    ++seen;
    if (current.index == last.index) {
      messages.push_back("MARKER");
      end_reached = true;
    }
    return EntryResponse::More;
  }

  EntryResponse operator()(const StorableString& message, IDX_TS current, IDX_TS last) {
    return operator()(message.s, current, last);
  }

  TerminationResponse Terminate() { return TerminationResponse::Terminate; }
};
}  // namespace persistence_test

TEST(PersistenceLayer, MemoryOnly) {
  using namespace persistence_test;
  using IMPL = current::persistence::MemoryOnly<std::string>;
  static_assert(current::ss::IsPublisher<IMPL>::value, "");
  static_assert(current::ss::IsEntryPublisher<IMPL, std::string>::value, "");
  static_assert(current::ss::IsStreamPublisher<IMPL, std::string>::value, "");
  static_assert(!current::ss::IsPublisher<int>::value, "");
  static_assert(!current::ss::IsEntryPublisher<IMPL, int>::value, "");
  static_assert(!current::ss::IsStreamPublisher<IMPL, int>::value, "");
  using PersistenceTestListener = current::ss::StreamSubscriber<PersistenceTestListenerImpl, std::string>;
  static_assert(current::ss::IsSubscriber<PersistenceTestListener>::value, "");
  static_assert(current::ss::IsEntrySubscriber<PersistenceTestListener, std::string>::value, "");
  static_assert(current::ss::IsStreamSubscriber<PersistenceTestListener, std::string>::value, "");
  static_assert(!current::ss::IsSubscriber<int>::value, "");
  static_assert(!current::ss::IsEntrySubscriber<PersistenceTestListener, int>::value, "");
  static_assert(!current::ss::IsStreamSubscriber<PersistenceTestListener, int>::value, "");

  {
    IMPL impl;

    impl.Publish("foo");
    impl.Publish("bar");
    EXPECT_EQ(2u, impl.Size());

    current::WaitableTerminateSignal stop;
    PersistenceTestListener test_listener;
    std::thread t([&impl, &stop, &test_listener]() { impl.SyncScanAllEntries(stop, test_listener); });

    while (!test_listener.end_reached) {
      ;  // Spin lock.
    }

    impl.Publish("meh");
    EXPECT_EQ(3u, impl.Size());

    while (test_listener.seen < 3u) {
      ;  // Spin lock.
    }

    stop.SignalExternalTermination();
    t.join();

    EXPECT_EQ(3u, test_listener.seen);
    EXPECT_EQ("foo,bar,MARKER,meh,MARKER", Join(test_listener.messages, ","));
  }

  {
    // Obviously, no state is shared for `MemoryOnly` implementation.
    // The data starts from ground zero.
    IMPL impl;

    current::WaitableTerminateSignal stop;
    PersistenceTestListener test_listener;
    std::thread t([&impl, &stop, &test_listener]() { impl.SyncScanAllEntries(stop, test_listener); });

    impl.Publish("blah");

    while (test_listener.seen < 1u) {
      ;  // Spin lock.
    }

    stop.SignalExternalTermination();
    t.join();

    EXPECT_EQ(1u, test_listener.seen);
    EXPECT_EQ("blah,MARKER", Join(test_listener.messages, ","));
  }
}

TEST(PersistenceLayer, AppendToFile) {
  using namespace persistence_test;
  using IMPL = current::persistence::NewAppendToFile<StorableString>;
  static_assert(current::ss::IsPublisher<IMPL>::value, "");
  static_assert(current::ss::IsEntryPublisher<IMPL, StorableString>::value, "");
  static_assert(!current::ss::IsPublisher<int>::value, "");
  static_assert(!current::ss::IsEntryPublisher<IMPL, int>::value, "");
  using PersistenceTestListener = current::ss::StreamSubscriber<PersistenceTestListenerImpl, StorableString>;
  static_assert(current::ss::IsSubscriber<PersistenceTestListener>::value, "");
  static_assert(current::ss::IsEntrySubscriber<PersistenceTestListener, StorableString>::value, "");
  static_assert(current::ss::IsStreamSubscriber<PersistenceTestListener, StorableString>::value, "");
  static_assert(!current::ss::IsSubscriber<int>::value, "");
  static_assert(!current::ss::IsEntrySubscriber<PersistenceTestListener, int>::value, "");
  static_assert(!current::ss::IsStreamSubscriber<PersistenceTestListener, int>::value, "");

  const std::string persistence_file_name =
      current::FileSystem::JoinPath(FLAGS_persistence_test_tmpdir, "data");
  const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

  {
    IMPL impl(persistence_file_name);
    current::time::SetNow(std::chrono::microseconds(100));
    impl.Publish(StorableString("foo"));
    current::time::SetNow(std::chrono::microseconds(200));
    impl.Publish(std::move(StorableString("bar")));
    EXPECT_EQ(2u, impl.Size());

    current::WaitableTerminateSignal stop;
    PersistenceTestListener test_listener;
    std::thread t([&impl, &stop, &test_listener]() { impl.SyncScanAllEntries(stop, test_listener); });

    while (!test_listener.end_reached) {
      ;  // Spin lock.
    }

    current::time::SetNow(std::chrono::microseconds(500));
    impl.Publish(StorableString("meh"));
    EXPECT_EQ(3u, impl.Size());

    while (test_listener.seen < 3u) {
      ;  // Spin lock.
    }

    stop.SignalExternalTermination();
    t.join();

    EXPECT_EQ(3u, test_listener.seen);
    EXPECT_EQ("foo,bar,MARKER,meh,MARKER", Join(test_listener.messages, ","));
  }

  EXPECT_EQ(
      "{\"index\":1,\"us\":100}\t{\"s\":\"foo\"}\n"
      "{\"index\":2,\"us\":200}\t{\"s\":\"bar\"}\n"
      "{\"index\":3,\"us\":500}\t{\"s\":\"meh\"}\n",
      current::FileSystem::ReadFileAsString(persistence_file_name));

  {
    // Confirm that the data has been saved and can be replayed.
    IMPL impl(persistence_file_name);

    current::WaitableTerminateSignal stop;
    PersistenceTestListener test_listener;
    std::thread t([&impl, &stop, &test_listener]() { impl.SyncScanAllEntries(stop, test_listener); });

    while (!test_listener.end_reached) {
      ;  // Spin lock.
    }

    EXPECT_EQ("foo,bar,meh,MARKER", Join(test_listener.messages, ","));

    impl.Publish(StorableString("blah"));

    while (test_listener.seen < 4u) {
      ;  // Spin lock.
    }

    stop.SignalExternalTermination();
    t.join();

    EXPECT_EQ(4u, test_listener.seen);
    EXPECT_EQ("foo,bar,meh,MARKER,blah,MARKER", Join(test_listener.messages, ","));
  }
}

TEST(PersistenceLayer, Exceptions) {
  using namespace persistence_test;
  using IMPL = current::persistence::NewAppendToFile<StorableString>;
  using current::ss::IndexAndTimestamp;
  using current::persistence::MalformedEntryDuringReplayException;
  using current::persistence::InconsistentIndexException;
  using current::persistence::InconsistentTimestampException;

  const std::string persistence_file_name =
      current::FileSystem::JoinPath(FLAGS_persistence_test_tmpdir, "data");

  // Malformed entry during replay.
  {
    const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);
    current::FileSystem::WriteStringToFile("Malformed entry", persistence_file_name.c_str());
    EXPECT_THROW(IMPL impl(persistence_file_name), MalformedEntryDuringReplayException);
  }
  // Inconsistent index during replay.
  {
    const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);
    current::FileSystem::WriteStringToFile(
        "{\"index\":1,\"us\":100}\t{\"s\":\"foo\"}\n"
        "{\"index\":1,\"us\":200}\t{\"s\":\"bar\"}\n",
        persistence_file_name.c_str());
    EXPECT_THROW(IMPL impl(persistence_file_name), InconsistentIndexException);
  }
  // Inconsistent timestamp during replay.
  {
    const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);
    current::FileSystem::WriteStringToFile(
        "{\"index\":1,\"us\":100}\t{\"s\":\"foo\"}\n"
        "{\"index\":2,\"us\":50}\t{\"s\":\"bar\"}\n",
        persistence_file_name.c_str());
    EXPECT_THROW(IMPL impl(persistence_file_name), InconsistentTimestampException);
  }
  {
    const auto file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);
    current::FileSystem::WriteStringToFile("{\"index\":1,\"us\":100}\t{\"s\":\"foo\"}\n",
                                           persistence_file_name.c_str());
    IMPL impl(persistence_file_name);
    StorableString entry("bar");
    // `PublishReplayed()` with consistent timestamp, but inconsistent index.
    EXPECT_THROW(impl.PublishReplayed(entry, IndexAndTimestamp(3u, std::chrono::microseconds(200))),
                 InconsistentIndexException);
    // `PublishReplayed()` with consistent index, but inconsistent timestamp.
    EXPECT_THROW(impl.PublishReplayed(entry, IndexAndTimestamp(2u, std::chrono::microseconds(100))),
                 InconsistentTimestampException);
    // `PublishReplayed()` with absolutely consistent `IndexAndTimestamp`.
    EXPECT_NO_THROW(impl.PublishReplayed(entry, IndexAndTimestamp(2u, std::chrono::microseconds(200))));
    EXPECT_EQ(
        "{\"index\":1,\"us\":100}\t{\"s\":\"foo\"}\n"
        "{\"index\":2,\"us\":200}\t{\"s\":\"bar\"}\n",
        current::FileSystem::ReadFileAsString(persistence_file_name));
  }
}
