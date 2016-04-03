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

#include "sherlock.h"

#include <string>
#include <atomic>
#include <thread>

#include "../TypeSystem/struct.h"
#include "../TypeSystem/variant.h"

#include "../Blocks/HTTP/api.h"
#include "../Blocks/Persistence/persistence.h"

#include "../Bricks/strings/strings.h"

#include "../Bricks/time/chrono.h"
#include "../Bricks/dflags/dflags.h"

#include "../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_int32(sherlock_http_test_port, PickPortForUnitTest(), "Local port to use for Sherlock unit test.");
DEFINE_string(sherlock_test_tmpdir, ".current", "Local path for the test to create temporary files in.");

namespace sherlock_unittest {

using current::strings::Join;
using current::strings::Printf;
using current::ss::EntryResponse;
using current::ss::TerminationResponse;

// The records we work with.
CURRENT_STRUCT(Record) {
  CURRENT_FIELD(x, int);
  CURRENT_CONSTRUCTOR(Record)(int x = 0) : x(x) {}
};

CURRENT_STRUCT(RecordWithTimestamp) {
  CURRENT_FIELD(s, std::string);
  CURRENT_FIELD(t, std::chrono::microseconds);
  CURRENT_CONSTRUCTOR(RecordWithTimestamp)(std::string s = "",
                                           std::chrono::microseconds t = std::chrono::microseconds(0ull))
      : s(s), t(t) {}
  CURRENT_USE_FIELD_AS_TIMESTAMP(t);
};

CURRENT_STRUCT(AnotherRecord) {
  CURRENT_FIELD(y, int);
  CURRENT_CONSTRUCTOR(AnotherRecord)(int y = 0) : y(y) {}
};

// Struct `Data` should be outside struct `SherlockTestProcessor`,
// since the latter is `std::move`-d away in some tests.
struct Data final {
  std::atomic_bool subscriber_alive_;
  std::atomic_size_t seen_;
  std::string results_;
  Data() : subscriber_alive_(false), seen_(0u) {}
};

// Struct `SherlockTestProcessor` handles the entries that tests subscribe to.
struct SherlockTestProcessorImpl {
  Data& data_;                  // Initialized in constructor.
  const bool allow_terminate_;  // Initialized in constructor.
  const bool with_idx_ts_;      // Initialized in constructor.
  static std::string kTerminateStr;
  size_t max_to_process_ = static_cast<size_t>(-1);

  SherlockTestProcessorImpl() = delete;

  explicit SherlockTestProcessorImpl(Data& data, bool allow_terminate, bool with_idx_ts = false)
      : data_(data), allow_terminate_(allow_terminate), with_idx_ts_(with_idx_ts) {
    assert(!data_.subscriber_alive_);
    data_.subscriber_alive_ = true;
  }

  ~SherlockTestProcessorImpl() {
    assert(data_.subscriber_alive_);
    data_.subscriber_alive_ = false;
  }

  void SetMax(size_t cap) { max_to_process_ = cap; }

  EntryResponse operator()(const Record& entry, idxts_t current, idxts_t last) {
    if (!data_.results_.empty()) {
      data_.results_ += ",";
    }
    if (with_idx_ts_) {
      data_.results_ += Printf(
          "[%llu:%llu,%llu:%llu] %i", current.index, current.us.count(), last.index, last.us.count(), entry.x);
    } else {
      data_.results_ += current::ToString(entry.x);
    }
    ++data_.seen_;
    if (data_.seen_ < max_to_process_) {
      return EntryResponse::More;
    } else {
      return EntryResponse::Done;
    }
  }

  static EntryResponse EntryResponseIfNoMorePassTypeFilter() { return EntryResponse::Done; }

  TerminationResponse Terminate() {
    if (!data_.results_.empty()) {
      data_.results_ += ",";
    }
    data_.results_ += kTerminateStr;
    if (allow_terminate_) {
      return TerminationResponse::Terminate;
    } else {
      return TerminationResponse::Wait;
    }
  }
};

std::string SherlockTestProcessorImpl::kTerminateStr = "TERMINATE";

using SherlockTestProcessor =
    current::ss::StreamSubscriber<SherlockTestProcessorImpl, sherlock_unittest::Record>;

static_assert(current::ss::IsStreamSubscriber<SherlockTestProcessor, sherlock_unittest::Record>::value, "");

// LCOV_EXCL_START
// This function is executed by the test, but its branching is unpredictable, so `LCOV_EXCL` it. -- D.K.
inline bool CompareValuesMixedWithTerminate(const std::string& lhs,
                                            std::vector<std::string> values,
                                            const std::string& terminate_str) {
  if (values.empty()) {
    return lhs.empty();
  }
  auto it = values.begin();
  for (size_t i = 0; i <= values.size(); ++i) {
    it = values.insert(it, terminate_str);
    if (lhs == Join(values, ',')) {
      return true;
    }
    it = values.erase(it);
    ++it;
  }
  return (lhs == Join(values, ','));
}
// LCOV_EXCL_STOP

}  // namespace sherlock_unittest

TEST(Sherlock, SubscribeAndProcessThreeEntries) {
  using namespace sherlock_unittest;

  auto foo_stream = current::sherlock::Stream<Record>();
  current::time::SetNow(std::chrono::microseconds(1));
  foo_stream.Publish(1, std::chrono::microseconds(10));
  foo_stream.Publish(2, std::chrono::microseconds(20));
  foo_stream.Publish(3, std::chrono::microseconds(30));
  Data d;
  {
    ASSERT_FALSE(d.subscriber_alive_);
    SherlockTestProcessor p(d, false, true);
    ASSERT_TRUE(d.subscriber_alive_);
    p.SetMax(3u);
    foo_stream.Subscribe(p).Join();  // `.Join()` blocks this thread waiting for three entries.
    EXPECT_EQ(3u, d.seen_);
    ASSERT_TRUE(d.subscriber_alive_);
  }
  ASSERT_FALSE(d.subscriber_alive_);

  const std::vector<std::string> expected_values{"[0:10,2:30] 1", "[1:20,2:30] 2", "[2:30,2:30] 3"};
  // A careful condition, since the subscriber may process some or all entries before going out of scope.
  EXPECT_TRUE(
      CompareValuesMixedWithTerminate(d.results_, expected_values, SherlockTestProcessor::kTerminateStr))
      << Join(expected_values, ',') << " != " << d.results_;
}

TEST(Sherlock, SubscribeSynchronously) {
  using namespace sherlock_unittest;

  auto bar_stream = current::sherlock::Stream<Record>();
  current::time::SetNow(std::chrono::microseconds(40));
  bar_stream.Publish(4);
  current::time::SetNow(std::chrono::microseconds(50));
  bar_stream.Publish(5);
  current::time::SetNow(std::chrono::microseconds(60));
  bar_stream.Publish(6);
  Data d;
  ASSERT_FALSE(d.subscriber_alive_);
  std::unique_ptr<SherlockTestProcessor> p(new SherlockTestProcessor(d, false, true));
  ASSERT_TRUE(d.subscriber_alive_);
  p->SetMax(3u);
  bar_stream.Subscribe(std::move(p)).Join();  // `.Join()` blocks this thread waiting for three entries.
  EXPECT_EQ(3u, d.seen_);
  EXPECT_FALSE(d.subscriber_alive_);

  const std::vector<std::string> expected_values{"[0:40,2:60] 4", "[1:50,2:60] 5", "[2:60,2:60] 6"};
  // A careful condition, since the subscriber may process some or all entries before going out of scope.
  EXPECT_TRUE(
      CompareValuesMixedWithTerminate(d.results_, expected_values, SherlockTestProcessor::kTerminateStr))
      << Join(expected_values, ',') << " != " << d.results_;
}

TEST(Sherlock, SubscribeAsynchronously) {
  using namespace sherlock_unittest;

  auto bar_stream = current::sherlock::Stream<Record>();
  current::time::SetNow(std::chrono::microseconds(40));
  bar_stream.Publish(4);
  current::time::SetNow(std::chrono::microseconds(50));
  bar_stream.Publish(5);
  current::time::SetNow(std::chrono::microseconds(60));
  bar_stream.Publish(6);
  Data d;
  std::unique_ptr<SherlockTestProcessor> p(new SherlockTestProcessor(d, false, true));
  p->SetMax(4u);
  bar_stream.Subscribe(std::move(p)).Detach();  // `.Detach()` results in the subscriber running on its own.
  while (d.seen_ < 3u) {
    ;  // Spin lock.
  }
  EXPECT_EQ(3u, d.seen_);
  const std::vector<std::string> expected_values{"[0:40,2:60] 4", "[1:50,2:60] 5", "[2:60,2:60] 6"};
  EXPECT_EQ(Join(expected_values, ','), d.results_);  // No `TERMINATE` for an asyncronous subscriber.
  EXPECT_TRUE(d.subscriber_alive_);
  current::time::SetNow(std::chrono::microseconds(70));
  bar_stream.Publish(42);  // Need the 4th entry for the async subscriber to terminate.
  while (d.subscriber_alive_) {
    ;  // Spin lock.
  }
}

TEST(Sherlock, SubscribeHandleGoesOutOfScopeBeforeAnyProcessing) {
  using namespace sherlock_unittest;

  auto baz_stream = current::sherlock::Stream<Record>();
  std::atomic_bool wait(true);
  std::thread delayed_publish_thread([&baz_stream, &wait]() {
    while (wait) {
      ;  // Spin lock.
    }
    current::time::SetNow(std::chrono::microseconds(1));
    baz_stream.Publish(7);
    current::time::SetNow(std::chrono::microseconds(2));
    baz_stream.Publish(8);
    current::time::SetNow(std::chrono::microseconds(3));
    baz_stream.Publish(9);
  });
  {
    Data d;
    SherlockTestProcessor p(d, true);
    // NOTE: plain `baz_stream.Subscribe(p);` will fail with exception
    // in the destructor of `SyncSubscriberScope`.
    baz_stream.Subscribe(p).Join();
    EXPECT_EQ(0u, d.seen_);
  }
  {
    Data d;
    SherlockTestProcessor p(d, true);
    auto scope = baz_stream.Subscribe(p);
    scope.Join();
    EXPECT_EQ(0u, d.seen_);
  }
  wait = false;
  delayed_publish_thread.join();
}

TEST(Sherlock, SubscribeProcessedThreeEntriesBecauseWeWaitInTheScope) {
  using namespace sherlock_unittest;

  auto meh_stream = current::sherlock::Stream<Record>();
  current::time::SetNow(std::chrono::microseconds(1));
  meh_stream.Publish(10);
  current::time::SetNow(std::chrono::microseconds(2));
  meh_stream.Publish(11);
  current::time::SetNow(std::chrono::microseconds(3));
  meh_stream.Publish(12);
  Data d;
  SherlockTestProcessor p(d, true);
  {
    auto scope = meh_stream.Subscribe(p);
    {
      auto scope2 = std::move(scope);
      {
        auto scope3 = std::move(scope2);
        while (d.seen_ < 3u) {
          ;  // Spin lock.
        }
        // If the next line is commented out, an unrecoverable exception
        // will be thrown in the destructor of `SyncSubscriberScope`.
        scope3.Join();
      }
    }
  }
  EXPECT_EQ(3u, d.seen_);
  EXPECT_EQ("10,11,12," + SherlockTestProcessor::kTerminateStr, d.results_);
}

namespace sherlock_unittest {

// Collector class for `SubscribeToStreamViaHTTP` test.
struct RecordsCollectorImpl {
  std::atomic_size_t count_;
  std::vector<std::string>& data_;

  RecordsCollectorImpl() = delete;
  explicit RecordsCollectorImpl(std::vector<std::string>& data) : count_(0u), data_(data) {}

  EntryResponse operator()(const RecordWithTimestamp& entry, idxts_t current, idxts_t) {
    data_.push_back(JSON(current) + '\t' + JSON(entry) + '\n');
    ++count_;
    return EntryResponse::More;
  }

  static EntryResponse EntryResponseIfNoMorePassTypeFilter() { return EntryResponse::More; }

  TerminationResponse Terminate() { return TerminationResponse::Terminate; }
};

using RecordsCollector = current::ss::StreamSubscriber<RecordsCollectorImpl, RecordWithTimestamp>;

}  // namespace sherlock_unittest

TEST(Sherlock, SubscribeToStreamViaHTTP) {
  using namespace sherlock_unittest;

  auto exposed_stream = current::sherlock::Stream<RecordWithTimestamp>();
  // Expose stream via HTTP.
  HTTP(FLAGS_sherlock_http_test_port).ResetAllHandlers();
  HTTP(FLAGS_sherlock_http_test_port).Register("/exposed", exposed_stream);
  const std::string base_url = Printf("http://localhost:%d/exposed", FLAGS_sherlock_http_test_port);

  {
    // Test that verbs other than "GET" result in '405 Method not allowed' error.
    EXPECT_EQ(405, static_cast<int>(HTTP(POST(base_url + "?n=1", "")).code));
    EXPECT_EQ(405, static_cast<int>(HTTP(PUT(base_url + "?n=1", "")).code));
    EXPECT_EQ(405, static_cast<int>(HTTP(DELETE(base_url + "?n=1")).code));
  }
  {
    // Request with `?nowait` works even if stream is empty.
    const auto result = HTTP(GET(base_url + "?nowait"));
    EXPECT_EQ(200, static_cast<int>(result.code));
    EXPECT_EQ("", result.body);
  }
  {
    // `?sizeonly` returns "0" since the stream is empty.
    const auto result = HTTP(GET(base_url + "?sizeonly"));
    EXPECT_EQ(200, static_cast<int>(result.code));
    EXPECT_EQ("0\n", result.body);
  }
  {
    // HEAD is equivalent to `?sizeonly` except the size is returned in the header.
    const auto result = HTTP(HEAD(base_url));
    EXPECT_EQ(200, static_cast<int>(result.code));
    EXPECT_EQ("", result.body);
    ASSERT_TRUE(result.headers.Has("X-Current-Stream-Size"));
    EXPECT_EQ("0", result.headers.Get("X-Current-Stream-Size"));
  }

  // Publish four records.
  // { "s[0]", "s[1]", "s[2]", "s[3]" } at timestamps 100, 200, 300 and 400 microseconds respectively.
  current::time::SetNow(std::chrono::microseconds(100));
  exposed_stream.Publish(RecordWithTimestamp("s[0]", std::chrono::microseconds(100)));  // Emplace().
  current::time::SetNow(std::chrono::microseconds(200));
  exposed_stream.Publish(RecordWithTimestamp("s[1]", std::chrono::microseconds(200)));  // Emplace().
  current::time::SetNow(std::chrono::microseconds(300));
  exposed_stream.Publish(RecordWithTimestamp("s[2]", std::chrono::microseconds(300)));  // Emplace().
  current::time::SetNow(std::chrono::microseconds(400));
  exposed_stream.Publish(RecordWithTimestamp("s[3]", std::chrono::microseconds(400)));  // Emplace().
  const auto now = std::chrono::microseconds(500);
  current::time::SetNow(now);

  std::vector<std::string> s;
  RecordsCollector collector(s);
  {
    // Explicitly confirm the return type for ths scope is what is should be, no `auto`. -- D.K.
    // This is to fight the trouble with an `unique_ptr<*, NullDeleter>` mistakenly emerging due to internals.
    current::sherlock::StreamImpl<RecordWithTimestamp>::SyncSubscriberScope<RecordsCollector> scope(
        exposed_stream.Subscribe(collector));
    while (collector.count_ < 4u) {
      ;  // Spin lock.
    }
    scope.Join();
  }
  EXPECT_EQ(4u, s.size());

  {
    const auto result = HTTP(GET(base_url + "?sizeonly"));
    EXPECT_EQ(200, static_cast<int>(result.code));
    EXPECT_EQ("4\n", result.body);
  }
  {
    const auto result = HTTP(HEAD(base_url));
    EXPECT_EQ(200, static_cast<int>(result.code));
    EXPECT_EQ("", result.body);
    ASSERT_TRUE(result.headers.Has("X-Current-Stream-Size"));
    EXPECT_EQ("4", result.headers.Get("X-Current-Stream-Size"));
  }

  // Test `n`.
  EXPECT_EQ(s[0], HTTP(GET(base_url + "?n=1")).body);
  EXPECT_EQ(s[0] + s[1], HTTP(GET(base_url + "?n=2")).body);
  EXPECT_EQ(s[0] + s[1] + s[2], HTTP(GET(base_url + "?n=3")).body);
  EXPECT_EQ(s[0] + s[1] + s[2] + s[3], HTTP(GET(base_url + "?n=4")).body);
  // Test `n` + `nowait`.
  EXPECT_EQ(s[0] + s[1] + s[2] + s[3], HTTP(GET(base_url + "?n=100&nowait")).body);

  // Test `since` + `nowait`.
  // All entries since the timestamp of the last entry.
  EXPECT_EQ(s[3], HTTP(GET(base_url + "?since=400&nowait")).body);
  // All entries since the moment 1 us later than the second entry timestamp.
  EXPECT_EQ(s[2] + s[3], HTTP(GET(base_url + "?since=201&nowait")).body);
  // All entries since the timestamp of the second entry.
  EXPECT_EQ(s[1] + s[2] + s[3], HTTP(GET(base_url + "?since=200&nowait")).body);
  // All entries since the timestamp in the future.
  EXPECT_EQ("", HTTP(GET(base_url + "?since=5000&nowait")).body);

  // Test `since` + `n`.
  // One entry since the timestamp of the last entry.
  EXPECT_EQ(s[3], HTTP(GET(base_url + "?since=400&n=1")).body);
  // Two entries since the timestamp of the first entry.
  EXPECT_EQ(s[0] + s[1], HTTP(GET(base_url + "?since=100&n=2")).body);

  // Test `recent` + `nowait`.
  // All entries since (now - 400 us).
  EXPECT_EQ(
      s[3],
      HTTP(GET(base_url + "?nowait&recent=" + current::ToString(now - std::chrono::microseconds(400)))).body);
  // All entries since (now - 300 us).
  EXPECT_EQ(
      s[2] + s[3],
      HTTP(GET(base_url + "?nowait&recent=" + current::ToString(now - std::chrono::microseconds(300)))).body);
  // All entries since (now - 100 us).
  EXPECT_EQ(
      s[0] + s[1] + s[2] + s[3],
      HTTP(GET(base_url + "?nowait&recent=" + current::ToString(now - std::chrono::microseconds(100)))).body);
  // Large `recent` value => all entries.
  EXPECT_EQ(s[0] + s[1] + s[2] + s[3], HTTP(GET(base_url + "?nowait&recent=10000")).body);

  // Test `recent` + `n`.
  // Two entries since (now - 101 us).
  EXPECT_EQ(
      s[1] + s[2],
      HTTP(GET(base_url + "?n=2&recent=" + current::ToString(now - std::chrono::microseconds(101)))).body);
  // Three entries with large `recent` value.
  EXPECT_EQ(s[0] + s[1] + s[2], HTTP(GET(base_url + "?n=3&recent=10000")).body);

  // Test `i` + `nowait`
  // All entries from `index = 3`.
  EXPECT_EQ(s[3], HTTP(GET(base_url + "?i=3&nowait")).body);
  // All entries from `index = 1`.
  EXPECT_EQ(s[1] + s[2] + s[3], HTTP(GET(base_url + "?i=1&nowait")).body);

  // Test `i` + `n`
  // One entry from `index = 0`.
  EXPECT_EQ(s[0], HTTP(GET(base_url + "?i=0&n=1")).body);
  // Two entry from `index = 2`.
  EXPECT_EQ(s[2] + s[3], HTTP(GET(base_url + "?i=2&n=2")).body);

  // Test `tail` + `nowait`
  // Last three entries.
  EXPECT_EQ(s[1] + s[2] + s[3], HTTP(GET(base_url + "?tail=3&nowait")).body);
  // Large `tail` value => all entries.
  EXPECT_EQ(s[0] + s[1] + s[2] + s[3], HTTP(GET(base_url + "?tail=50&nowait")).body);

  // Test `tail` + `n`
  // One entry starting from the 4th from the end.
  EXPECT_EQ(s[0], HTTP(GET(base_url + "?tail=4&n=1")).body);
  // Two entries starting from the 3rd from the end.
  EXPECT_EQ(s[1] + s[2], HTTP(GET(base_url + "?tail=3&n=2")).body);

  // Test `tail` + `i` + `nowait`.
  // More strict constraint by `tail`.
  EXPECT_EQ(s[3], HTTP(GET(base_url + "?tail=1&i=0&nowait")).body);
  // More strict constraint by `i`.
  EXPECT_EQ(s[3], HTTP(GET(base_url + "?tail=4&i=3&nowait")).body);
  // Nonexistent `i`.
  EXPECT_EQ("", HTTP(GET(base_url + "?tail=4&i=10&nowait")).body);

  // Test `tail` + `i` + `n`.
  // More strict constraint by `tail`.
  EXPECT_EQ(s[2], HTTP(GET(base_url + "?tail=2&i=0&n=1")).body);
  // More strict constraint by `i`.
  EXPECT_EQ(s[1], HTTP(GET(base_url + "?tail=4&i=1&n=1")).body);

  // Test `period`.
  // Start from the first entry with the `period` less than 100.
  EXPECT_EQ(s[0], HTTP(GET(base_url + "?period=99")).body);
  // Start 1 us later than the first entry with the `period` less than 200.
  EXPECT_EQ(s[1] + s[2], HTTP(GET(base_url + "?since=101&period=199")).body);
  // Start 1 us later than the first entry with the `period` equal to 200.
  EXPECT_EQ(s[1] + s[2] + s[3], HTTP(GET(base_url + "?since=101&period=200&nowait")).body);
  // Start 1 us later than the first entry with the `period` equal to 200 and limit to one entry.
  EXPECT_EQ(s[1], HTTP(GET(base_url + "?since=101&period=200&n=1")).body);
  // Start from the third entry from the end with the `period` less than 100.
  EXPECT_EQ(s[1], HTTP(GET(base_url + "?tail=3&period=99")).body);

  // Test `?stop_after_bytes=...`'
  // Request exactly the size of the first entry.
  EXPECT_EQ(s[0], HTTP(GET(base_url + "?stop_after_bytes=42")).body);
  // Request slightly more max bytes than the size of the first entry.
  EXPECT_EQ(s[0] + s[1], HTTP(GET(base_url + "?stop_after_bytes=50")).body);
  // Request exactly the size of the first two entries.
  EXPECT_EQ(s[0] + s[1], HTTP(GET(base_url + "?stop_after_bytes=84")).body);
  // Request with the capacity large enough to hold all the entries.
  EXPECT_EQ(s[0] + s[1] + s[2] + s[3], HTTP(GET(base_url + "?stop_after_bytes=100000&nowait")).body);

  // TODO(dkorolev): Add tests that add data while the chunked response is in progress.
  // TODO(dkorolev): Unregister the exposed endpoint and free its handler. It's hanging out there now...
  // TODO(dkorolev): Add tests that the endpoint is not unregistered until its last client is done. (?)
}

const std::string sherlock_golden_data =
    "{\"index\":0,\"us\":100}\t{\"x\":1}\n"
    "{\"index\":1,\"us\":200}\t{\"x\":2}\n"
    "{\"index\":2,\"us\":300}\t{\"x\":3}\n";

TEST(Sherlock, PersistsToFile) {
  using namespace sherlock_unittest;

  const std::string persistence_file_name = current::FileSystem::JoinPath(FLAGS_sherlock_test_tmpdir, "data");
  const auto persistence_file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

  auto persisted = current::sherlock::Stream<Record, current::persistence::File>(persistence_file_name);

  current::time::SetNow(std::chrono::microseconds(100u));
  persisted.Publish(1);
  current::time::SetNow(std::chrono::microseconds(200u));
  persisted.Publish(2);
  current::time::SetNow(std::chrono::microseconds(300u));
  persisted.Publish(3);

  // This spin lock is unnecessary as publishing is synchronous as of now. -- D.K.
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

  auto parsed = current::sherlock::Stream<Record, current::persistence::File>(persistence_file_name);

  Data d;
  {
    ASSERT_FALSE(d.subscriber_alive_);
    SherlockTestProcessor p(d, false, true);
    ASSERT_TRUE(d.subscriber_alive_);
    p.SetMax(3u);
    parsed.Subscribe(p).Join();  // `.Join()` blocks this thread waiting for three entries.
    EXPECT_EQ(3u, d.seen_);
    ASSERT_TRUE(d.subscriber_alive_);
  }
  ASSERT_FALSE(d.subscriber_alive_);

  const std::vector<std::string> expected_values{"[0:100,2:300] 1", "[1:200,2:300] 2", "[2:300,2:300] 3"};
  // A careful condition, since the subscriber may process some or all entries before going out of scope.
  EXPECT_TRUE(
      CompareValuesMixedWithTerminate(d.results_, expected_values, SherlockTestProcessor::kTerminateStr))
      << d.results_;
}

TEST(Sherlock, SubscribeWithFilterByType) {
  using namespace sherlock_unittest;

  struct CollectorImpl {
    CollectorImpl() = delete;
    CollectorImpl(const CollectorImpl&) = delete;
    CollectorImpl(CollectorImpl&&) = delete;

    explicit CollectorImpl(size_t expected_count) : expected_count_(expected_count) {}

    EntryResponse operator()(const Variant<Record, AnotherRecord>& entry, idxts_t, idxts_t) {
      results_.push_back(JSON<JSONFormat::Minimalistic>(entry));
      return results_.size() == expected_count_ ? EntryResponse::Done : EntryResponse::More;
    }

    EntryResponse operator()(const Record& record, idxts_t, idxts_t) {
      results_.push_back("X=" + current::ToString(record.x));
      return results_.size() == expected_count_ ? EntryResponse::Done : EntryResponse::More;
    }

    EntryResponse operator()(const AnotherRecord& another_record, idxts_t, idxts_t) {
      results_.push_back("Y=" + current::ToString(another_record.y));
      return results_.size() == expected_count_ ? EntryResponse::Done : EntryResponse::More;
    }

    TerminationResponse Terminate() const { return TerminationResponse::Wait; }

    static EntryResponse EntryResponseIfNoMorePassTypeFilter() { return EntryResponse::More; }

    std::vector<std::string> results_;
    const size_t expected_count_;
  };

  auto stream = current::sherlock::Stream<Variant<Record, AnotherRecord>>();
  for (int i = 1; i <= 5; ++i) {
    current::time::SetNow(std::chrono::microseconds(i));
    if (i & 1) {
      stream.Publish(Record(i));
    } else {
      stream.Publish(AnotherRecord(i));
    }
  }

  {
    using Collector = current::ss::StreamSubscriber<CollectorImpl, Variant<Record, AnotherRecord>>;
    static_assert(current::ss::IsStreamSubscriber<Collector, Variant<Record, AnotherRecord>>::value, "");
    static_assert(!current::ss::IsStreamSubscriber<Collector, Record>::value, "");
    static_assert(!current::ss::IsStreamSubscriber<Collector, AnotherRecord>::value, "");

    Collector c(5);
    stream.Subscribe(c).Join();
    EXPECT_EQ(
        "{\"Record\":{\"x\":1}} {\"AnotherRecord\":{\"y\":2}} {\"Record\":{\"x\":3}} "
        "{\"AnotherRecord\":{\"y\":4}} {\"Record\":{\"x\":5}}",
        Join(c.results_, ' '));
  }

  {
    using Collector = current::ss::StreamSubscriber<CollectorImpl, Record>;
    static_assert(!current::ss::IsStreamSubscriber<Collector, Variant<Record, AnotherRecord>>::value, "");
    static_assert(current::ss::IsStreamSubscriber<Collector, Record>::value, "");
    static_assert(!current::ss::IsStreamSubscriber<Collector, AnotherRecord>::value, "");

    Collector c(3);
    stream.Subscribe<Record>(c).Join();
    EXPECT_EQ("X=1 X=3 X=5", Join(c.results_, ' '));
  }

  {
    using Collector = current::ss::StreamSubscriber<CollectorImpl, AnotherRecord>;
    static_assert(!current::ss::IsStreamSubscriber<Collector, Variant<Record, AnotherRecord>>::value, "");
    static_assert(!current::ss::IsStreamSubscriber<Collector, Record>::value, "");
    static_assert(current::ss::IsStreamSubscriber<Collector, AnotherRecord>::value, "");

    Collector c(2);
    stream.Subscribe<AnotherRecord>(c).Join();
    EXPECT_EQ("Y=2 Y=4", Join(c.results_, ' '));
  }
}
