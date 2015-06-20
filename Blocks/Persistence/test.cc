/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#include "persistence.h"

#include "../../Bricks/dflags/dflags.h"
#include "../../Bricks/file/file.h"
#include "../../Bricks/strings/join.h"

#include "../../Bricks/util/clone.h"

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_string(persistence_test_tmpdir, ".current", "Local path for the test to create temporary files in.");

using blocks::MemoryOnly;
using blocks::AppendToFile;
using blocks::DispatchEntryByRValue;

using bricks::DefaultCloneFunction;

using bricks::FileSystem;
using bricks::strings::Join;

struct CerealizableString {
  using CEREAL_BASE_TYPE = CerealizableString;
  std::string s;
  CerealizableString(const std::string& s = "") : s(s) {}
  CerealizableString(const char* s) : s(s) {}
  template <typename A>
  void serialize(A& ar) {
    ar(CEREAL_NVP(s));
  }
};

struct PersistenceTestListener {
  std::atomic_size_t seen;
  std::atomic_bool replay_done;

  std::vector<std::string> messages;

  PersistenceTestListener() : seen(0u), replay_done(false) {}

  void operator()(const std::string& message) {
    messages.push_back(message);
    ++seen;
  }

  void operator()(const CerealizableString& message) { operator()(message.s); }

  void ReplayDone() {
    messages.push_back("MARKER");
    replay_done = true;
  }
};

TEST(PersistenceLayer, MemoryOnly) {
  {
    MemoryOnly<std::string> impl(DefaultCloneFunction<std::string>());

    impl.Publish("foo");
    impl.Publish("bar");

    std::atomic_bool stop(false);
    PersistenceTestListener test_listener;
    std::thread t([&impl, &stop, &test_listener]() { impl.SyncScanAllEntries(stop, test_listener); });

    while (!test_listener.replay_done) {
      ;  // Spin lock.
    }

    impl.Publish("meh");

    while (test_listener.seen < 3u) {
      ;  // Spin lock.
    }

    stop = true;
    t.join();

    EXPECT_EQ(3u, test_listener.seen);
    EXPECT_EQ("foo,bar,MARKER,meh", Join(test_listener.messages, ","));
  }

  {
    // Obviously, no state is shared for `MemoryOnly` implementation.
    // The data starts from ground zero.
    MemoryOnly<std::string> impl(DefaultCloneFunction<std::string>());

    std::atomic_bool stop(false);
    PersistenceTestListener test_listener;
    std::thread t([&impl, &stop, &test_listener]() { impl.SyncScanAllEntries(stop, test_listener); });

    while (!test_listener.replay_done) {
      ;  // Spin lock.
    }

    impl.Publish("blah");

    while (test_listener.seen < 1u) {
      ;  // Spin lock.
    }

    stop = true;
    t.join();

    EXPECT_EQ(1u, test_listener.seen);
    EXPECT_EQ("MARKER,blah", Join(test_listener.messages, ","));
  }
}

TEST(PersistenceLayer, AppendToFile) {
  const std::string fn = FileSystem::JoinPath(FLAGS_persistence_test_tmpdir, "data");
  FileSystem::RmFile(fn, FileSystem::RmFileParameters::Silent);
  const auto file_remover = FileSystem::ScopedRmFile(fn);

  {
    AppendToFile<CerealizableString> impl(DefaultCloneFunction<CerealizableString>(), fn);

    impl.Publish("foo");
    impl.Publish("bar");

    std::atomic_bool stop(false);
    PersistenceTestListener test_listener;
    std::thread t([&impl, &stop, &test_listener]() { impl.SyncScanAllEntries(stop, test_listener); });

    while (!test_listener.replay_done) {
      ;  // Spin lock.
    }

    impl.Publish("meh");

    while (test_listener.seen < 3u) {
      ;  // Spin lock.
    }

    stop = true;
    t.join();

    EXPECT_EQ(3u, test_listener.seen);
    EXPECT_EQ("foo,bar,MARKER,meh", Join(test_listener.messages, ","));
  }

  {
    // Confirm that the data has been saved and can be replayed.
    AppendToFile<CerealizableString> impl(DefaultCloneFunction<CerealizableString>(), fn);

    std::atomic_bool stop(false);
    PersistenceTestListener test_listener;
    std::thread t([&impl, &stop, &test_listener]() { impl.SyncScanAllEntries(stop, test_listener); });

    while (!test_listener.replay_done) {
      ;  // Spin lock.
    }

    EXPECT_EQ("foo,bar,meh,MARKER", Join(test_listener.messages, ","));

    impl.Publish("blah");

    while (test_listener.seen < 4u) {
      ;  // Spin lock.
    }

    stop = true;
    t.join();

    EXPECT_EQ(4u, test_listener.seen);
    EXPECT_EQ("foo,bar,meh,MARKER,blah", Join(test_listener.messages, ","));
  }
}

TEST(PersistenceLayer, RespectsCustomCloneFunction) {
  struct BASE {
    virtual std::string AsString() = 0;
    virtual std::unique_ptr<BASE> Clone() = 0;
  };
  struct A : BASE {
    std::string AsString() override { return "A"; }
    std::unique_ptr<BASE> Clone() override { return std::unique_ptr<BASE>(new A()); }
  };
  struct B : BASE {
    std::string AsString() override { return "B"; }
    std::unique_ptr<BASE> Clone() override { return std::unique_ptr<BASE>(new B()); }
  };
  MemoryOnly<std::unique_ptr<BASE>> test_clone(
      [](const std::unique_ptr<BASE>& input) { return input->Clone(); });

  test_clone.Publish(make_unique<A>());
  test_clone.Publish(make_unique<B>());

  std::vector<std::string> results;
  size_t counter = 0u;
  std::atomic_bool stop(false);
  test_clone.SyncScanAllEntries(stop, [&results, &counter](std::unique_ptr<BASE>&& e) {
    results.push_back(e->AsString());
    ++counter;
    return counter < 2u;
  });

  EXPECT_EQ(2u, counter);
  EXPECT_EQ("A,B", Join(results, ","));
}
