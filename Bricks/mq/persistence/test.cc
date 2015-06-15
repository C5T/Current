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

#include "../interface/interface.h"

#include "../../dflags/dflags.h"
#include "../../file/file.h"
#include "../../strings/join.h"
//#include "../../strings/printf.h"

#include "../../../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_string(persistence_test_tmpdir, ".current", "Local path for the test to create temporary files in.");

using bricks::persistence::MemoryOnly;

using bricks::FileSystem;
using bricks::strings::Join;

using bricks::mq::DispatchEntryByRValue;

struct PersistenceTestListener {
  std::atomic_size_t seen;
  std::atomic_bool replay_done;

  std::vector<std::string> messages;

  PersistenceTestListener() : seen(0u), replay_done(false) {}
  
  void operator()(const std::string& message) {
    messages.push_back(message);
    ++seen;
  }

  void ReplayDone() {
    messages.push_back("MARKER");
    replay_done = true;
  }
};

TEST(PersistenceLayer, MemoryOnly) {
  MemoryOnly<std::string> memory_only;

  memory_only.Publish("foo");
  memory_only.Publish("bar");

  std::atomic_bool stop(false);
  PersistenceTestListener test_listener;
  std::thread t([&memory_only, &stop, &test_listener]() {
    memory_only.SyncScanAllEntries(stop, test_listener);
  });

  while (!test_listener.replay_done) {
    ;  // Spin lock.
  }

  memory_only.Publish("meh");

  while (test_listener.seen < 3u) {
    ;  // Spin lock.
  }

  stop = true;
  t.join();

  EXPECT_EQ(3u, test_listener.seen);
  EXPECT_EQ("foo,bar,MARKER,meh", Join(test_listener.messages, ","));
}

TEST(PersistenceLayer, RespectsCustomCloneFunction) {
  struct BASE {
    virtual std::string AsString() = 0;
    virtual std::unique_ptr<BASE> Clone() = 0;
  };
  struct A : BASE {
    std::string AsString() override { return "A"; }
    std::unique_ptr<BASE> Clone() override {
      return std::unique_ptr<BASE>(new A());
    }
  };
  struct B : BASE {
    std::string AsString() override { return "B"; }
    std::unique_ptr<BASE> Clone() override {
      return std::unique_ptr<BASE>(new B());
    }
  };
  MemoryOnly<std::unique_ptr<BASE>> memory_only([](const std::unique_ptr<BASE>& input) { return input->Clone(); });

  memory_only.Publish(make_unique<A>());
  memory_only.Publish(make_unique<B>());

  std::vector<std::string> results;
  size_t counter = 0u;
  std::atomic_bool stop(false);
  memory_only.SyncScanAllEntries(stop, [&results, &counter](std::unique_ptr<BASE>&& e) {
    results.push_back(e->AsString());
    ++counter;
    return counter < 2u;
  });

  EXPECT_EQ(2u, counter);
  EXPECT_EQ("A,B", Join(results, ","));
}

/*
    FileSystem::RmFile(fn, FileSystem::RmFileParameters::Silent);
      const auto file_remover = FileSystem::ScopedRmFile(fn);
    const std::string fn1 = FileSystem::JoinPath(FLAGS_persistence_test_tmpdir, "one");
    const std::string fn2 = FileSystem::JoinPath(FLAGS_persistence_test_tmpdir, "two");

    FileSystem::RmFile(fn1, FileSystem::RmFileParameters::Silent);
    EXPECT_EQ(5, 2 * 2);
*/
