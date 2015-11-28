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

#define BRICKS_MOCK_TIME

#include "sherlock.h"

#include <string>
#include <atomic>
#include <thread>

#include "../TypeSystem/struct.h"

#include "../Blocks/HTTP/api.h"
#include "../Blocks/Persistence/persistence.h"

#include "../Bricks/strings/strings.h"

#if 0
#include "../Bricks/cerealize/cerealize.h"
#endif

#include "../Bricks/time/chrono.h"
#include "../Bricks/dflags/dflags.h"

#include "../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_int32(sherlock_http_test_port, 8090, "Local port to use for Sherlock unit test.");
DEFINE_string(sherlock_test_tmpdir, ".current", "Local path for the test to create temporary files in.");

using std::string;
using std::atomic_bool;
using std::atomic_size_t;
using std::thread;
using std::this_thread::sleep_for;
using std::chrono::milliseconds;

using current::strings::Join;
using current::strings::Printf;
using current::strings::ToString;

namespace sherlock_unittest {

// The records we work with.
CURRENT_STRUCT(Record) {
  CURRENT_FIELD(x, int);
  CURRENT_CONSTRUCTOR(Record)(int x = 0) : x(x) {}
};

CURRENT_STRUCT(RecordWithTimestamp) {
  // TODO(dkorolev): Make the `EPOCH_MILLISECONDS` type serializable.
  CURRENT_FIELD(s, std::string);
  CURRENT_FIELD(t, std::chrono::microseconds);
  CURRENT_CONSTRUCTOR(RecordWithTimestamp)(std::string s = "",
                                           std::chrono::microseconds t = std::chrono::microseconds(0ull))
      : s(s), t(t) {}
  CURRENT_TIMESTAMP(t);
};

}  // namespace sherlock_unittest

// Struct `Data` should be outside struct `SherlockTestProcessor`,
// since the latter is `std::move`-d away in some tests.
struct Data final {
  atomic_bool listener_alive_;
  atomic_size_t seen_;
  string results_;
  Data() : listener_alive_(false), seen_(0u) {}
};

// Struct `SherlockTestProcessor` handles the entries that tests subscribe to.
struct SherlockTestProcessor final {
  Data& data_;                  // Initialized in constructor.
  const bool allow_terminate_;  // Initialized in constructor.
  size_t max_to_process_ = static_cast<size_t>(-1);

  SherlockTestProcessor() = delete;

  explicit SherlockTestProcessor(Data& data, bool allow_terminate)
      : data_(data), allow_terminate_(allow_terminate) {
    assert(!data_.listener_alive_);
    data_.listener_alive_ = true;
  }

  ~SherlockTestProcessor() {
    assert(data_.listener_alive_);
    data_.listener_alive_ = false;
  }

  SherlockTestProcessor& SetMax(size_t cap) {
    max_to_process_ = cap;
    return *this;
  }

  inline bool operator()(sherlock_unittest::Record&& entry) {
    if (!data_.results_.empty()) {
      data_.results_ += ",";
    }
    data_.results_ += ToString(entry.x);
    ++data_.seen_;
    return data_.seen_ < max_to_process_;
  }

  inline bool Terminate() {
    if (!data_.results_.empty()) {
      data_.results_ += ",";
    }
    data_.results_ += "TERMINATE";
    return allow_terminate_;
  }
};

TEST(Sherlock, SubscribeAndProcessThreeEntries) {
  using namespace sherlock_unittest;

  auto foo_stream = sherlock::Stream<Record>("foo");
  foo_stream.Publish(1);
  foo_stream.Publish(2);
  foo_stream.Publish(3);
  Data d;
  {
    ASSERT_FALSE(d.listener_alive_);
    SherlockTestProcessor p(d, false);
    ASSERT_TRUE(d.listener_alive_);
    foo_stream.SyncSubscribe(p.SetMax(3u)).Join();  // `.Join()` blocks this thread waiting for three entries.
    EXPECT_EQ(3u, d.seen_);
    ASSERT_TRUE(d.listener_alive_);
  }
  ASSERT_FALSE(d.listener_alive_);

  // A careful condition, since the listener may process some or all entries before going out of scope.
  EXPECT_TRUE((d.results_ == "TERMINATE,1,2,3") || (d.results_ == "1,TERMINATE,2,3") ||
              (d.results_ == "1,2,TERMINATE,3") || (d.results_ == "1,2,3,TERMINATE") || (d.results_ == "1,2,3"))
      << d.results_;
}

TEST(Sherlock, SubscribeAndProcessThreeEntriesByUniquePtr) {
  using namespace sherlock_unittest;

  auto bar_stream = sherlock::Stream<Record>("bar");
  bar_stream.Publish(4);
  bar_stream.Publish(5);
  bar_stream.Publish(6);
  Data d;
  ASSERT_FALSE(d.listener_alive_);
  std::unique_ptr<SherlockTestProcessor> p(new SherlockTestProcessor(d, false));
  ASSERT_TRUE(d.listener_alive_);
  p->SetMax(3u);
  bar_stream.AsyncSubscribe(std::move(p)).Join();  // `.Join()` blocks this thread waiting for three entries.
  EXPECT_EQ(3u, d.seen_);
  while (d.listener_alive_) {
    ;  // Spin lock.
  }

  // A careful condition, since the listener may process some or all entries before going out of scope.
  EXPECT_TRUE((d.results_ == "TERMINATE,4,5,6") || (d.results_ == "4,TERMINATE,5,6") ||
              (d.results_ == "4,5,TERMINATE,6") || (d.results_ == "4,5,6,TERMINATE") || (d.results_ == "4,5,6"))
      << d.results_;
}

TEST(Sherlock, AsyncSubscribeAndProcessThreeEntriesByUniquePtr) {
  using namespace sherlock_unittest;

  auto bar_stream = sherlock::Stream<Record>("bar");
  bar_stream.Publish(4);
  bar_stream.Publish(5);
  bar_stream.Publish(6);
  Data d;
  std::unique_ptr<SherlockTestProcessor> p(new SherlockTestProcessor(d, false));
  p->SetMax(4u);
  bar_stream.AsyncSubscribe(std::move(p)).Detach();  // `.Detach()` results in the listener running on its own.
  while (d.seen_ < 3u) {
    ;  // Spin lock.
  }
  EXPECT_EQ(3u, d.seen_);
  EXPECT_EQ("4,5,6", d.results_);  // No `TERMINATE` for an asyncronous listener.
  EXPECT_TRUE(d.listener_alive_);
  bar_stream.Publish(42);  // Need the 4th entry for the async listener to terminate.
  while (d.listener_alive_) {
    ;  // Spin lock.
  }
}

TEST(Sherlock, SubscribeHandleGoesOutOfScopeBeforeAnyProcessing) {
  using namespace sherlock_unittest;

  auto baz_stream = sherlock::Stream<Record>("baz");
  atomic_bool wait(true);
  thread delayed_publish_thread([&baz_stream, &wait]() {
    while (wait) {
      ;  // Spin lock.
    }
    baz_stream.Publish(7);
    baz_stream.Publish(8);
    baz_stream.Publish(9);
  });
  {
    Data d;
    SherlockTestProcessor p(d, true);
    // NOTE: plain `baz_stream.SyncSubscribe(p);` will fail with exception
    // in the destructor of `SyncListenerScope`.
    baz_stream.SyncSubscribe(p).Join();
    EXPECT_EQ(0u, d.seen_);
  }
  {
    Data d;
    SherlockTestProcessor p(d, true);
    auto scope = baz_stream.SyncSubscribe(p);
    scope.Join();
    EXPECT_EQ(0u, d.seen_);
  }
  wait = false;
  delayed_publish_thread.join();
}

TEST(Sherlock, SubscribeProcessedThreeEntriesBecauseWeWaitInTheScope) {
  using namespace sherlock_unittest;

  auto meh_stream = sherlock::Stream<Record>("meh");
  meh_stream.Publish(10);
  meh_stream.Publish(11);
  meh_stream.Publish(12);
  Data d;
  SherlockTestProcessor p(d, true);
  {
    auto scope = meh_stream.SyncSubscribe(p);
    {
      auto scope2 = std::move(scope);
      {
        auto scope3 = std::move(scope2);
        while (d.seen_ < 3u) {
          ;  // Spin lock.
        }
        // If the next line is commented out, an unrecoverable exception
        // will be thrown in the destructor of `SyncListenerScope`.
        scope3.Join();
      }
    }
  }
  EXPECT_EQ(3u, d.seen_);
  EXPECT_EQ("10,11,12,TERMINATE", d.results_);
}

TEST(Sherlock, SubscribeToStreamViaHTTP) {
  using namespace sherlock_unittest;

  // Publish four records.
  // { "s[0]", "s[1]", "s[2]", "s[3]" } 40, 30, 20 and 10 seconds ago respectively.
  auto exposed_stream = sherlock::Stream<RecordWithTimestamp>("exposed");
  const std::chrono::microseconds now = current::time::Now();
  exposed_stream.Emplace("s[0]", now - std::chrono::microseconds(40000));
  exposed_stream.Emplace("s[1]", now - std::chrono::microseconds(30000));
  exposed_stream.Emplace("s[2]", now - std::chrono::microseconds(20000));
  exposed_stream.Emplace("s[3]", now - std::chrono::microseconds(10000));

  // Collect them and store as strings.
  // Required since we don't mock time for this test, and therefore can't do exact match.
  struct RecordsCollector {
    atomic_size_t count_;
    std::vector<std::string>& data_;

    RecordsCollector() = delete;
    explicit RecordsCollector(std::vector<std::string>& data) : count_(0u), data_(data) {}

    inline bool operator()(const RecordWithTimestamp& entry, size_t index, size_t total) {
      static_cast<void>(index);
      static_cast<void>(total);
      data_.push_back(JSON(entry) + '\n');
      ++count_;
      return true;
    }
  };

  std::vector<std::string> s;
  RecordsCollector collector(s);
  {
    auto scope = exposed_stream.SyncSubscribe(collector);
    while (collector.count_ < 4u) {
      ;  // Spin lock.
    }
    scope.Join();
  }
  EXPECT_EQ(4u, s.size());

  HTTP(FLAGS_sherlock_http_test_port).ResetAllHandlers();
  HTTP(FLAGS_sherlock_http_test_port).Register("/exposed", exposed_stream);

  // Test `?n=...`.
  EXPECT_EQ(s[3], HTTP(GET(Printf("http://localhost:%d/exposed?n=1", FLAGS_sherlock_http_test_port))).body);

  EXPECT_EQ(s[2] + s[3],
            HTTP(GET(Printf("http://localhost:%d/exposed?n=2", FLAGS_sherlock_http_test_port))).body);
  EXPECT_EQ(s[1] + s[2] + s[3],
            HTTP(GET(Printf("http://localhost:%d/exposed?n=3", FLAGS_sherlock_http_test_port))).body);
  EXPECT_EQ(s[0] + s[1] + s[2] + s[3],
            HTTP(GET(Printf("http://localhost:%d/exposed?n=4", FLAGS_sherlock_http_test_port))).body);
  // `?n={>4}` will block forever.

  // Test `?cap=...`.
  EXPECT_EQ(s[0] + s[1] + s[2] + s[3],
            HTTP(GET(Printf("http://localhost:%d/exposed?cap=4", FLAGS_sherlock_http_test_port))).body);
  EXPECT_EQ(s[0] + s[1],
            HTTP(GET(Printf("http://localhost:%d/exposed?cap=2", FLAGS_sherlock_http_test_port))).body);
  EXPECT_EQ(s[0], HTTP(GET(Printf("http://localhost:%d/exposed?cap=1", FLAGS_sherlock_http_test_port))).body);

  // Test `?recent=...`, have to use `?cap=...`.
  EXPECT_EQ(
      s[3],
      HTTP(GET(Printf("http://localhost:%d/exposed?cap=1&recent=15000", FLAGS_sherlock_http_test_port))).body);
  EXPECT_EQ(
      s[2],
      HTTP(GET(Printf("http://localhost:%d/exposed?cap=1&recent=25000", FLAGS_sherlock_http_test_port))).body);
  EXPECT_EQ(
      s[1],
      HTTP(GET(Printf("http://localhost:%d/exposed?cap=1&recent=35000", FLAGS_sherlock_http_test_port))).body);
  EXPECT_EQ(
      s[0],
      HTTP(GET(Printf("http://localhost:%d/exposed?cap=1&recent=45000", FLAGS_sherlock_http_test_port))).body);

  EXPECT_EQ(
      s[2] + s[3],
      HTTP(GET(Printf("http://localhost:%d/exposed?cap=2&recent=25000", FLAGS_sherlock_http_test_port))).body);
  EXPECT_EQ(
      s[1] + s[2],
      HTTP(GET(Printf("http://localhost:%d/exposed?cap=2&recent=35000", FLAGS_sherlock_http_test_port))).body);
  EXPECT_EQ(
      s[0] + s[1],
      HTTP(GET(Printf("http://localhost:%d/exposed?cap=2&recent=45000", FLAGS_sherlock_http_test_port))).body);

  EXPECT_EQ(
      s[1] + s[2] + s[3],
      HTTP(GET(Printf("http://localhost:%d/exposed?cap=3&recent=35000", FLAGS_sherlock_http_test_port))).body);
  EXPECT_EQ(
      s[0] + s[1] + s[2],
      HTTP(GET(Printf("http://localhost:%d/exposed?cap=3&recent=45000", FLAGS_sherlock_http_test_port))).body);

  // TODO(dkorolev): Add tests that add data while the chunked response is in progress.
  // TODO(dkorolev): Unregister the exposed endpoint and free its handler. It's hanging out there now...
  // TODO(dkorolev): Add tests that the endpoint is not unregistered until its last client is done. (?)
}

const std::string sherlock_golden_data = "{\"x\":1}\n{\"x\":2}\n{\"x\":3}\n";

TEST(Sherlock, PersistsToFile) {
  using namespace sherlock_unittest;

  const std::string persistence_file_name = current::FileSystem::JoinPath(FLAGS_sherlock_test_tmpdir, "data");
  const auto persistence_file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

  auto persisted =
      sherlock::Stream<Record, blocks::persistence::NewAppendToFile>("persisted", persistence_file_name);

  persisted.Publish(1);
  persisted.Publish(2);
  persisted.Publish(3);

  while (current::FileSystem::GetFileSize(persistence_file_name) != sherlock_golden_data.size()) {
    ;  // Spin lock.
  }

  EXPECT_EQ(sherlock_golden_data, current::FileSystem::ReadFileAsString(persistence_file_name));
}

TEST(Sherlock, ParsesFromFile) {
  using namespace sherlock_unittest;

  const std::string persistence_file_name = current::FileSystem::JoinPath(FLAGS_sherlock_test_tmpdir, "data");
  const auto persistence_file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);
  current::FileSystem::WriteStringToFile(sherlock_golden_data, persistence_file_name.c_str());

  auto parsed = sherlock::Stream<Record, blocks::persistence::NewAppendToFile>("parsed", persistence_file_name);

  Data d;
  {
    ASSERT_FALSE(d.listener_alive_);
    SherlockTestProcessor p(d, false);
    ASSERT_TRUE(d.listener_alive_);
    parsed.SyncSubscribe(p.SetMax(3u)).Join();  // `.Join()` blocks this thread waiting for three entries.
    EXPECT_EQ(3u, d.seen_);
    ASSERT_TRUE(d.listener_alive_);
  }
  ASSERT_FALSE(d.listener_alive_);

  // A careful condition, since the listener may process some or all entries before going out of scope.
  EXPECT_TRUE((d.results_ == "TERMINATE,1,2,3") || (d.results_ == "1,TERMINATE,2,3") ||
              (d.results_ == "1,2,TERMINATE,3") || (d.results_ == "1,2,3,TERMINATE") || (d.results_ == "1,2,3"))
      << d.results_;
}
