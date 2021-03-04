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
#define CURRENT_BUILD_WITH_PARANOIC_RUNTIME_CHECKS

#include "stream.h"
#include "replicator.h"

#include <string>
#include <atomic>
#include <thread>

#include "../typesystem/struct.h"
#include "../typesystem/variant.h"

#include "../blocks/http/api.h"
#include "../blocks/persistence/memory.h"
#include "../blocks/persistence/file.h"

#include "../bricks/strings/strings.h"

#include "../bricks/time/chrono.h"
#include "../bricks/dflags/dflags.h"

#include "../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_int32(stream_http_test_port, PickPortForUnitTest(), "Local port to use for Stream unit test.");
DEFINE_string(stream_test_tmpdir, ".current", "Local path for the test to create temporary files in.");

namespace stream_unittest {

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

// Struct `Data` should be outside struct `StreamTestProcessor`,
// since the latter is `std::move`-d away in some tests.
struct Data final {
  std::atomic_bool subscriber_alive_;
  std::atomic_size_t seen_;
  std::string results_;
  std::chrono::microseconds head_;
  Data() : subscriber_alive_(false), seen_(0u) {}
};

// Struct `StreamTestProcessor` handles the entries that tests subscribe to.
struct StreamTestProcessorImpl {
  Data& data_;                  // Initialized in constructor.
  const bool allow_terminate_;  // Initialized in constructor.
  const bool with_idx_ts_;      // Initialized in constructor.
  static std::string kTerminateStr;
  size_t max_to_process_ = static_cast<size_t>(-1);
  std::atomic_bool wait_;

  StreamTestProcessorImpl() = delete;

  explicit StreamTestProcessorImpl(Data& data, bool allow_terminate, bool with_idx_ts = false)
      : data_(data), allow_terminate_(allow_terminate), with_idx_ts_(with_idx_ts), wait_(false) {
    CURRENT_ASSERT(!data_.subscriber_alive_);
    data_.subscriber_alive_ = true;
  }

  ~StreamTestProcessorImpl() {
    CURRENT_ASSERT(data_.subscriber_alive_);
    data_.subscriber_alive_ = false;
  }

  void SetMax(size_t cap) { max_to_process_ = cap; }
  void SetWait(bool wait = true) { wait_ = wait; }

  EntryResponse operator()(const Record& entry, idxts_t current, idxts_t last) {
    while (wait_) {
      std::this_thread::yield();
    }
    if (!data_.results_.empty()) {
      data_.results_ += ",";
    }
    if (with_idx_ts_) {
      data_.results_ += Printf("[%llu:%llu,%llu:%llu] %i",
                               static_cast<unsigned long long>(current.index),
                               static_cast<unsigned long long>(current.us.count()),
                               static_cast<unsigned long long>(last.index),
                               static_cast<unsigned long long>(last.us.count()),
                               entry.x);
    } else {
      data_.results_ += current::ToString(entry.x);
    }
    data_.head_ = current.us;
    ++data_.seen_;
    if (data_.seen_ < max_to_process_) {
      return EntryResponse::More;
    } else {
      return EntryResponse::Done;
    }
  }

  EntryResponse operator()(const std::string& raw_log_line, uint64_t current_index, idxts_t last) {
    while (wait_) {
      std::this_thread::yield();
    }
    if (!data_.results_.empty()) {
      data_.results_ += ",";
    }
    const auto tab_pos = raw_log_line.find('\t');
    CURRENT_ASSERT(tab_pos != std::string::npos);
    const auto idxts = ParseJSON<idxts_t>(raw_log_line.substr(0, tab_pos));
    const auto entry = ParseJSON<Record>(raw_log_line.c_str() + tab_pos + 1);
    CURRENT_ASSERT(current_index == idxts.index);
    if (with_idx_ts_) {
      data_.results_ += Printf("[%llu:%llu,%llu:%llu] %i",
                               static_cast<unsigned long long>(current_index),
                               static_cast<unsigned long long>(idxts.us.count()),
                               static_cast<unsigned long long>(last.index),
                               static_cast<unsigned long long>(last.us.count()),
                               entry.x);
    } else {
      data_.results_ += current::ToString(entry.x);
    }
    data_.head_ = idxts.us;
    ++data_.seen_;
    if (data_.seen_ < max_to_process_) {
      return EntryResponse::More;
    } else {
      return EntryResponse::Done;
    }
  }

  EntryResponse operator()(std::chrono::microseconds ts) {
    while (wait_) {
      std::this_thread::yield();
    }
    data_.head_ = ts;
    ++data_.seen_;
    if (data_.seen_ < max_to_process_) {
      return EntryResponse::More;
    } else {
      return EntryResponse::Done;
    }
  }

  static EntryResponse EntryResponseIfNoMorePassTypeFilter() { return EntryResponse::Done; }

  TerminationResponse Terminate() {
    while (wait_) {
      std::this_thread::yield();
    }
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

std::string StreamTestProcessorImpl::kTerminateStr = "TERMINATE";

using StreamTestProcessor = current::ss::StreamSubscriber<StreamTestProcessorImpl, stream_unittest::Record>;

static_assert(current::ss::IsStreamSubscriber<StreamTestProcessor, stream_unittest::Record>::value, "");

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

}  // namespace stream_unittest

TEST(Stream, SubscribeAndProcessThreeEntries) {
  current::time::ResetToZero();

  using namespace stream_unittest;

  auto foo_stream = current::stream::Stream<Record>::CreateStream();
  auto foo_stream_publisher = foo_stream->BorrowPublisher();
  current::time::SetNow(std::chrono::microseconds(1));
  foo_stream_publisher->Publish(Record(1), std::chrono::microseconds(10));
  foo_stream_publisher->Publish(Record(2), std::chrono::microseconds(20));
  foo_stream_publisher->Publish(Record(3), std::chrono::microseconds(30));
  Data d;
  Data d_unchecked;
  {
    ASSERT_FALSE(d.subscriber_alive_);
    ASSERT_FALSE(d_unchecked.subscriber_alive_);
    StreamTestProcessor p(d, false, true);
    StreamTestProcessor p_unchecked(d_unchecked, false, true);
    ASSERT_TRUE(d.subscriber_alive_);
    ASSERT_TRUE(d_unchecked.subscriber_alive_);
    p.SetMax(3u);
    p_unchecked.SetMax(3u);
    foo_stream->Subscribe(p);  // With no return value collection to capture the scope, it's a blocking call.
    EXPECT_EQ(3u, d.seen_);
    ASSERT_TRUE(d.subscriber_alive_);
    foo_stream->SubscribeUnchecked(p_unchecked);
    EXPECT_EQ(3u, d_unchecked.seen_);
    ASSERT_TRUE(d_unchecked.subscriber_alive_);
  }
  ASSERT_FALSE(d.subscriber_alive_);
  ASSERT_FALSE(d_unchecked.subscriber_alive_);

  const std::vector<std::string> expected_values{"[0:10,2:30] 1", "[1:20,2:30] 2", "[2:30,2:30] 3"};
  const auto joined_expected_values = Join(expected_values, ',');
  // A careful condition, since the subscriber may process some or all entries before going out of scope.
  EXPECT_TRUE(CompareValuesMixedWithTerminate(d.results_, expected_values, StreamTestProcessor::kTerminateStr))
      << joined_expected_values << " != " << d.results_;
  EXPECT_TRUE(
      CompareValuesMixedWithTerminate(d_unchecked.results_, expected_values, StreamTestProcessor::kTerminateStr))
      << joined_expected_values << " != " << d_unchecked.results_;
}

TEST(Stream, SubscribeSynchronously) {
  current::time::ResetToZero();

  using namespace stream_unittest;

  auto bar_stream = current::stream::Stream<Record>::CreateStream();
  current::time::SetNow(std::chrono::microseconds(40));
  bar_stream->Publisher()->Publish(Record(4));
  current::time::SetNow(std::chrono::microseconds(50));
  bar_stream->Publisher()->Publish(Record(5));
  current::time::SetNow(std::chrono::microseconds(60));
  bar_stream->Publisher()->Publish(Record(6));
  Data d, d_unchecked;
  ASSERT_FALSE(d.subscriber_alive_);
  ASSERT_FALSE(d_unchecked.subscriber_alive_);

  {
    StreamTestProcessor p(d, false, true);
    StreamTestProcessor p_unchecked(d_unchecked, false, true);
    ASSERT_TRUE(d.subscriber_alive_);
    ASSERT_TRUE(d_unchecked.subscriber_alive_);
    p.SetMax(3u);
    p_unchecked.SetMax(3u);
    // As `.SetMax(3)` was called, blocks the thread until all three recods are processed.
    bar_stream->Subscribe(p);
    EXPECT_EQ(3u, d.seen_);
    ASSERT_TRUE(d.subscriber_alive_);
    bar_stream->SubscribeUnchecked(p_unchecked);
    EXPECT_EQ(3u, d_unchecked.seen_);
    ASSERT_TRUE(d_unchecked.subscriber_alive_);
  }

  EXPECT_FALSE(d.subscriber_alive_);
  EXPECT_FALSE(d_unchecked.subscriber_alive_);

  const std::vector<std::string> expected_values{"[0:40,2:60] 4", "[1:50,2:60] 5", "[2:60,2:60] 6"};
  const auto joined_expected_values = Join(expected_values, ',');
  // A careful condition, since the subscriber may process some or all entries before going out of scope.
  EXPECT_TRUE(CompareValuesMixedWithTerminate(d.results_, expected_values, StreamTestProcessor::kTerminateStr))
      << joined_expected_values << " != " << d.results_;
  EXPECT_TRUE(
      CompareValuesMixedWithTerminate(d_unchecked.results_, expected_values, StreamTestProcessor::kTerminateStr))
      << joined_expected_values << " != " << d_unchecked.results_;
}

TEST(Stream, SubscribeHandleGoesOutOfScopeBeforeAnyProcessing) {
  current::time::ResetToZero();

  using namespace stream_unittest;

  auto baz_stream = current::stream::Stream<Record>::CreateStream();
  std::atomic_bool wait(true);
  std::thread delayed_publish_thread([&baz_stream, &wait]() {
    while (wait) {
      std::this_thread::yield();
    }
    baz_stream->Publisher()->Publish(Record(7), std::chrono::microseconds(1));
    baz_stream->Publisher()->Publish(Record(8), std::chrono::microseconds(2));
    baz_stream->Publisher()->Publish(Record(9), std::chrono::microseconds(3));
  });
  {
    Data d, d_unchecked;
    StreamTestProcessor p(d, true);
    StreamTestProcessor p_unchecked(d_unchecked, true);
    baz_stream->Subscribe(p);
    EXPECT_EQ(0u, d.seen_);
    baz_stream->SubscribeUnchecked(p_unchecked);
    EXPECT_EQ(0u, d_unchecked.seen_);
  }
  wait = false;
  delayed_publish_thread.join();
  {
    Data d, d_unchecked;
    StreamTestProcessor p(d, false, true);
    StreamTestProcessor p_unchecked(d_unchecked, false, true);
    p.SetMax(3u);
    p_unchecked.SetMax(3u);
    baz_stream->Subscribe(p);
    EXPECT_EQ(3u, d.seen_);
    baz_stream->SubscribeUnchecked(p_unchecked);
    EXPECT_EQ(3u, d_unchecked.seen_);
  }
}

TEST(Stream, SubscribeProcessedThreeEntriesBecauseWeWaitInTheScope) {
  current::time::ResetToZero();

  using namespace stream_unittest;

  auto meh_stream = current::stream::Stream<Record>::CreateStream();
  current::time::SetNow(std::chrono::microseconds(1));
  meh_stream->Publisher()->Publish(Record(10));
  current::time::SetNow(std::chrono::microseconds(2));
  meh_stream->Publisher()->Publish(Record(11));
  current::time::SetNow(std::chrono::microseconds(3));
  meh_stream->Publisher()->Publish(Record(12));
  Data d, d_unchecked;
  StreamTestProcessor p(d, true);
  StreamTestProcessor p_unchecked(d_unchecked, true);
  p.SetWait();
  p_unchecked.SetWait();
  {
    auto scope1 = meh_stream->Subscribe(p);
    auto scope1_unchecked = meh_stream->SubscribeUnchecked(p_unchecked);
    EXPECT_TRUE(scope1);
    EXPECT_TRUE(scope1_unchecked);
    {
      auto scope2 = std::move(scope1);
      auto scope2_unchecked = std::move(scope1_unchecked);
      EXPECT_TRUE(scope2);
      EXPECT_TRUE(scope2_unchecked);
      EXPECT_FALSE(scope1);
      EXPECT_FALSE(scope1_unchecked);
      {
        current::stream::SubscriberScope scope3, scope3_unchecked;
        EXPECT_FALSE(scope3);
        EXPECT_FALSE(scope3_unchecked);
        EXPECT_TRUE(scope2);
        EXPECT_TRUE(scope2_unchecked);
        {
          scope3 = std::move(scope2);
          scope3_unchecked = std::move(scope2_unchecked);
          EXPECT_TRUE(scope3);
          EXPECT_TRUE(scope3_unchecked);
          EXPECT_FALSE(scope2);
          EXPECT_FALSE(scope2_unchecked);
          p.SetMax(3u);
          p_unchecked.SetMax(3u);
          EXPECT_EQ(0u, d.seen_);
          EXPECT_EQ(0u, d_unchecked.seen_);
          p.SetWait(false);
          p_unchecked.SetWait(false);
          while (d.seen_ < 3u || d_unchecked.seen_ < 3u) {
            std::this_thread::yield();
          }
          while (scope3 || scope3_unchecked) {
            std::this_thread::yield();
          }
        }
        EXPECT_FALSE(scope3);
        EXPECT_FALSE(scope3_unchecked);
      }
    }
  }
  EXPECT_EQ(3u, d.seen_);
  EXPECT_EQ("10,11,12", d.results_);
  EXPECT_EQ(3u, d_unchecked.seen_);
  EXPECT_EQ("10,11,12", d_unchecked.results_);
}

namespace stream_unittest {

// Collector class for `SubscribeToStreamViaHTTP` test.
template <typename ENTRY>
struct RecordsCollectorImpl {
  std::atomic_size_t count_;
  std::vector<std::string>& rows_;
  std::vector<std::string>& entries_;

  RecordsCollectorImpl() = delete;
  RecordsCollectorImpl(std::vector<std::string>& rows, std::vector<std::string>& entries)
      : count_(0u), rows_(rows), entries_(entries) {}

  EntryResponse operator()(const ENTRY& entry, idxts_t current, idxts_t) {
    rows_.push_back(JSON(current) + '\t' + JSON(entry) + '\n');
    entries_.push_back(JSON(entry) + '\n');
    ++count_;
    return EntryResponse::More;
  }

  EntryResponse operator()(std::chrono::microseconds) const { return EntryResponse::More; }

  static EntryResponse EntryResponseIfNoMorePassTypeFilter() { return EntryResponse::More; }

  TerminationResponse Terminate() { return TerminationResponse::Terminate; }
};

template <typename ENTRY>
struct RecordsUncheckedCollectorImpl {
  std::atomic_size_t count_;
  std::vector<std::string>& rows_;
  std::vector<std::string>& entries_;

  RecordsUncheckedCollectorImpl() = delete;
  RecordsUncheckedCollectorImpl(std::vector<std::string>& rows, std::vector<std::string>& entries)
      : count_(0u), rows_(rows), entries_(entries) {}

  EntryResponse operator()(const std::string& raw_log_line, uint64_t, idxts_t) {
    rows_.push_back(raw_log_line + '\n');
    const auto tab_pos = raw_log_line.find('\t');
    ++count_;
    if (tab_pos == std::string::npos) {
      entries_.push_back("Malformed row\n");
      return EntryResponse::More;
    }
    try {
      ParseJSON<idxts_t>(raw_log_line.substr(0, tab_pos));
    } catch (const current::InvalidJSONException&) {
      entries_.push_back("Malformed index and timestamp\n");
      return EntryResponse::More;
    }
    try {
      ParseJSON<ENTRY>(raw_log_line.substr(tab_pos + 1));
    } catch (const current::InvalidJSONException&) {
      entries_.push_back("Malformed entry\n");
      return EntryResponse::More;
    }
    entries_.push_back(raw_log_line.substr(tab_pos + 1) + '\n');
    return EntryResponse::More;
  }

  EntryResponse operator()(std::chrono::microseconds) const { return EntryResponse::More; }

  static EntryResponse EntryResponseIfNoMorePassTypeFilter() { return EntryResponse::More; }

  static TerminationResponse Terminate() { return TerminationResponse::Terminate; }
};

using RecordsWithTimestampCollector =
    current::ss::StreamSubscriber<RecordsCollectorImpl<RecordWithTimestamp>, RecordWithTimestamp>;
using RecordsCollector = current::ss::StreamSubscriber<RecordsCollectorImpl<Record>, Record>;
using RecordsUncheckedCollector = current::ss::StreamSubscriber<RecordsUncheckedCollectorImpl<Record>, Record>;

}  // namespace stream_unittest

TEST(Stream, SubscribeToStreamViaHTTP) {
  current::time::ResetToZero();

  using namespace stream_unittest;

  auto exposed_stream = current::stream::Stream<RecordWithTimestamp>::CreateStreamWithCustomNamespaceName(
      current::ss::StreamNamespaceName("Stream", "Transaction"));

  // Expose stream via HTTP.
  const std::string base_url = Printf("http://localhost:%d/exposed", FLAGS_stream_http_test_port);
  const auto scope =
      HTTP(FLAGS_stream_http_test_port).Register("/exposed", *exposed_stream) +
      HTTP(FLAGS_stream_http_test_port)
          .Register("/exposed_more", URLPathArgs::CountMask::None | URLPathArgs::CountMask::One, *exposed_stream);
  const std::string base_url_with_args = Printf("http://localhost:%d/exposed_more", FLAGS_stream_http_test_port);

  {
    // Test that verbs other than "GET" result in '405 Method not allowed' error.
    EXPECT_EQ(405, static_cast<int>(HTTP(POST(base_url + "?n=1", "")).code));
    EXPECT_EQ(405, static_cast<int>(HTTP(PUT(base_url + "?n=1", "")).code));
    EXPECT_EQ(405, static_cast<int>(HTTP(DELETE(base_url + "?n=1")).code));
  }
  {
    // Request with `?nowait` works even if stream is empty.
    const auto result = HTTP(GET(base_url + "?nowait&checked"));
    EXPECT_EQ(204, static_cast<int>(result.code));
    EXPECT_EQ("", result.body);
    const auto result_unchecked = HTTP(GET(base_url + "?nowait"));
    EXPECT_EQ(204, static_cast<int>(result_unchecked.code));
    EXPECT_EQ("", result_unchecked.body);
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
  {
    // `?schema` responds back with a top-level schema JSON, providing the name, TypeID,  and various serializations.
    // clang-format off
    const std::string golden_h =
        "// The `current.h` file is the one from `https://github.com/C5T/Current`.\n"
        "// Compile with `-std=c++11` or higher.\n"
        "\n"
        "#include \"current.h\"\n"
        "\n"
        "// clang-format off\n"
        "\n"
        "namespace current_userspace {\n"
        "\n"
        "#ifndef CURRENT_SCHEMA_FOR_T9205399982292878352\n"
        "#define CURRENT_SCHEMA_FOR_T9205399982292878352\n"
        "namespace t9205399982292878352 {\n"
        "CURRENT_STRUCT(RecordWithTimestamp) {\n"
        "  CURRENT_FIELD(s, std::string);\n"
        "  CURRENT_FIELD(t, std::chrono::microseconds);\n"
        "};\n"
        "}  // namespace t9205399982292878352\n"
        "#endif  // CURRENT_SCHEMA_FOR_T_9205399982292878352\n"
        "\n"
        "}  // namespace current_userspace\n"
        "\n"
        "#ifndef CURRENT_NAMESPACE_Stream_DEFINED\n"
        "#define CURRENT_NAMESPACE_Stream_DEFINED\n"
        "CURRENT_NAMESPACE(Stream) {\n"
        "  CURRENT_NAMESPACE_TYPE(RecordWithTimestamp, current_userspace::t9205399982292878352::RecordWithTimestamp);\n"
        "\n"
        "  // Privileged types.\n"
        "  CURRENT_NAMESPACE_TYPE(Transaction, current_userspace::t9205399982292878352::RecordWithTimestamp);\n"
        "};  // CURRENT_NAMESPACE(Stream)\n"
        "#endif  // CURRENT_NAMESPACE_Stream_DEFINED\n"
        "\n"
        "namespace current {\n"
        "namespace type_evolution {\n"
        "\n"
        "// Default evolution for struct `RecordWithTimestamp`.\n"
        "#ifndef DEFAULT_EVOLUTION_D14B346C83D304F4BCE8869962FCE6336A82DF792850CDE1D140D4AF7E93CE1D  // typename Stream::RecordWithTimestamp\n"
        "#define DEFAULT_EVOLUTION_D14B346C83D304F4BCE8869962FCE6336A82DF792850CDE1D140D4AF7E93CE1D  // typename Stream::RecordWithTimestamp\n"
        "template <typename CURRENT_ACTIVE_EVOLVER>\n"
        "struct Evolve<Stream, typename Stream::RecordWithTimestamp, CURRENT_ACTIVE_EVOLVER> {\n"
        "  using FROM = Stream;\n"
        "  template <typename INTO>\n"
        "  static void Go(const typename FROM::RecordWithTimestamp& from,\n"
        "                 typename INTO::RecordWithTimestamp& into) {\n"
        "      static_assert(::current::reflection::FieldCounter<typename INTO::RecordWithTimestamp>::value == 2,\n"
        "                    \"Custom evolver required.\");\n"
        "      CURRENT_COPY_FIELD(s);\n"
        "      CURRENT_COPY_FIELD(t);\n"
        "  }\n"
        "};\n"
        "#endif\n"
        "\n"
        "}  // namespace current::type_evolution\n"
        "}  // namespace current\n"
        "\n"
        "#if 0  // Boilerplate evolvers.\n"
        "\n"
        "CURRENT_STRUCT_EVOLVER(CustomEvolver, Stream, RecordWithTimestamp, {\n"
        "  CURRENT_COPY_FIELD(s);\n"
        "  CURRENT_COPY_FIELD(t);\n"
        "});\n"
        "\n"
        "#endif  // Boilerplate evolvers.\n"
        "\n"
        "// clang-format on\n";
    // clang-format on

    const std::string golden_fs =
        "// Usage: `fsharpi -r Newtonsoft.Json.dll schema.fs`\n"
        "(*\n"
        "open Newtonsoft.Json\n"
        "let inline JSON o = JsonConvert.SerializeObject(o)\n"
        "let inline ParseJSON (s : string) : 'T = JsonConvert.DeserializeObject<'T>(s)\n"
        "*)\n"
        "\n"
        "type RecordWithTimestamp = {\n"
        "  s : string\n"
        "  t : int64  // microseconds.\n"
        "}\n";
    {
      const auto result = HTTP(GET(base_url + "?schema"));
      EXPECT_EQ(200, static_cast<int>(result.code));
      const auto body = ParseJSON<current::stream::StreamSchema, JSONFormat::Minimalistic>(result.body);
      EXPECT_EQ("RecordWithTimestamp", body.type_name);
      EXPECT_EQ("9205399982292878352", current::ToString(body.type_id));
      ASSERT_TRUE(body.language.count("h"));
      EXPECT_EQ(golden_h, body.language.at("h"));
      ASSERT_TRUE(body.language.count("fs"));
      EXPECT_EQ(golden_fs, body.language.at("fs"));
    }
    {
      const auto result = HTTP(GET(base_url + "?schema=h"));
      EXPECT_EQ(200, static_cast<int>(result.code));
      EXPECT_EQ(golden_h, result.body);
    }
    {
      const auto result = HTTP(GET(base_url + "?schema=fs"));
      EXPECT_EQ(200, static_cast<int>(result.code));
      EXPECT_EQ(golden_fs, result.body);
    }
    {
      const auto result = HTTP(GET(base_url + "?schema=blah"));
      EXPECT_EQ(404, static_cast<int>(result.code));
      EXPECT_EQ(
          "blah",
          Value(ParseJSON<current::stream::StreamSchemaFormatNotFoundError>(result.body).unsupported_format_requested));
    }
    {
      // The `base_url` location does not have the URL argument registered, so it's a plain "standard" 404.
      const auto result = HTTP(GET(base_url + "/schema.h"));
      EXPECT_EQ(404, static_cast<int>(result.code));
    }
    {
      // The `base_url_with_args` location can return the schema by its "resource" name.
      const auto result = HTTP(GET(base_url_with_args + "/schema.h"));
      EXPECT_EQ(200, static_cast<int>(result.code));
      EXPECT_EQ(golden_h, result.body);
    }
    {
      // URL querystring parameter overrides the path.
      const auto result = HTTP(GET(base_url_with_args + "/schema.meh?schema=h"));
      EXPECT_EQ(200, static_cast<int>(result.code));
      EXPECT_EQ(golden_h, result.body);
    }
  }

  // Publish four records.
  // { "s[0]", "s[1]", "s[2]", "s[3]" } at timestamps 100, 200, 300 and 400 microseconds respectively.
  current::time::SetNow(std::chrono::microseconds(100));
  exposed_stream->Publisher()->Publish(RecordWithTimestamp("s[0]", std::chrono::microseconds(100)));
  current::time::SetNow(std::chrono::microseconds(200));
  exposed_stream->Publisher()->Publish(RecordWithTimestamp("s[1]", std::chrono::microseconds(200)));
  current::time::SetNow(std::chrono::microseconds(300));
  exposed_stream->Publisher()->Publish(RecordWithTimestamp("s[2]", std::chrono::microseconds(300)));
  current::time::SetNow(std::chrono::microseconds(400));
  exposed_stream->Publisher()->Publish(RecordWithTimestamp("s[3]", std::chrono::microseconds(400)));
  const auto now = std::chrono::microseconds(500);
  current::time::SetNow(now);

  std::vector<std::string> s;
  std::vector<std::string> e;
  RecordsWithTimestampCollector collector(s, e);
  {
    // Explicitly confirm the return type for ths scope is what is should be, no `auto`. -- D.K.
    // This is to fight the trouble with an `unique_ptr<*, NullDeleter>` mistakenly emerging due to internals.
    const current::stream::Stream<RecordWithTimestamp>::SubscriberScope<RecordsWithTimestampCollector> scope(
        exposed_stream->Subscribe(collector));
    while (collector.count_ < 4u) {
      std::this_thread::yield();
    }
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
  EXPECT_EQ(s[0], HTTP(GET(base_url + "?n=1&checked")).body);
  EXPECT_EQ(s[0], HTTP(GET(base_url + "?n=1")).body);
  EXPECT_EQ(s[0] + s[1], HTTP(GET(base_url + "?n=2&checked")).body);
  EXPECT_EQ(s[0] + s[1], HTTP(GET(base_url + "?n=2")).body);
  EXPECT_EQ(s[0] + s[1] + s[2], HTTP(GET(base_url + "?n=3&checked")).body);
  EXPECT_EQ(s[0] + s[1] + s[2], HTTP(GET(base_url + "?n=3")).body);
  EXPECT_EQ(s[0] + s[1] + s[2] + s[3], HTTP(GET(base_url + "?n=4&checked")).body);
  EXPECT_EQ(s[0] + s[1] + s[2] + s[3], HTTP(GET(base_url + "?n=4")).body);
  // Test `n` + `nowait`.
  EXPECT_EQ(s[0] + s[1] + s[2] + s[3], HTTP(GET(base_url + "?n=100&nowait&checked")).body);
  EXPECT_EQ(s[0] + s[1] + s[2] + s[3], HTTP(GET(base_url + "?n=100&nowait")).body);

  // Test `since` + `nowait`.
  // All entries since the timestamp of the last entry.
  EXPECT_EQ(s[3], HTTP(GET(base_url + "?since=400&nowait&checked")).body);
  EXPECT_EQ(s[3], HTTP(GET(base_url + "?since=400&nowait")).body);
  // All entries since the moment 1 us later than the second entry timestamp.
  EXPECT_EQ(s[2] + s[3], HTTP(GET(base_url + "?since=201&nowait&checked")).body);
  EXPECT_EQ(s[2] + s[3], HTTP(GET(base_url + "?since=201&nowait")).body);
  // All entries since the timestamp of the second entry.
  EXPECT_EQ(s[1] + s[2] + s[3], HTTP(GET(base_url + "?since=200&nowait&checked")).body);
  EXPECT_EQ(s[1] + s[2] + s[3], HTTP(GET(base_url + "?since=200&nowait")).body);
  // All entries since the timestamp in the future.
  EXPECT_EQ("", HTTP(GET(base_url + "?since=5000&nowait&checked")).body);
  EXPECT_EQ("", HTTP(GET(base_url + "?since=5000&nowait")).body);

  // Test `since` + `n`.
  // One entry since the timestamp of the last entry.
  EXPECT_EQ(s[3], HTTP(GET(base_url + "?since=400&n=1&checked")).body);
  EXPECT_EQ(s[3], HTTP(GET(base_url + "?since=400&n=1")).body);
  // Two entries since the timestamp of the first entry.
  EXPECT_EQ(s[0] + s[1], HTTP(GET(base_url + "?since=100&n=2&checked")).body);
  EXPECT_EQ(s[0] + s[1], HTTP(GET(base_url + "?since=100&n=2")).body);

  // Test `recent` + `nowait`.
  // All entries since (now - 400 us).
  EXPECT_EQ(
      s[3],
      HTTP(GET(base_url + "?nowait&checked&recent=" + current::ToString(now - std::chrono::microseconds(400)))).body);
  EXPECT_EQ(s[3],
            HTTP(GET(base_url + "?nowait&recent=" + current::ToString(now - std::chrono::microseconds(400)))).body);
  // All entries since (now - 300 us).
  EXPECT_EQ(
      s[2] + s[3],
      HTTP(GET(base_url + "?nowait&checked&recent=" + current::ToString(now - std::chrono::microseconds(300)))).body);
  EXPECT_EQ(s[2] + s[3],
            HTTP(GET(base_url + "?nowait&recent=" + current::ToString(now - std::chrono::microseconds(300)))).body);
  // All entries since (now - 100 us).
  EXPECT_EQ(
      s[0] + s[1] + s[2] + s[3],
      HTTP(GET(base_url + "?nowait&checked&recent=" + current::ToString(now - std::chrono::microseconds(100)))).body);
  EXPECT_EQ(s[0] + s[1] + s[2] + s[3],
            HTTP(GET(base_url + "?nowait&recent=" + current::ToString(now - std::chrono::microseconds(100)))).body);
  // Large `recent` value => all entries.
  EXPECT_EQ(s[0] + s[1] + s[2] + s[3], HTTP(GET(base_url + "?nowait&checked&recent=10000")).body);
  EXPECT_EQ(s[0] + s[1] + s[2] + s[3], HTTP(GET(base_url + "?nowait&recent=10000")).body);

  // Test `recent` + `n`.
  // Two entries since (now - 101 us).
  EXPECT_EQ(
      s[1] + s[2],
      HTTP(GET(base_url + "?n=2&checked&recent=" + current::ToString(now - std::chrono::microseconds(101)))).body);
  EXPECT_EQ(s[1] + s[2],
            HTTP(GET(base_url + "?n=2&recent=" + current::ToString(now - std::chrono::microseconds(101)))).body);
  // Three entries with large `recent` value.
  EXPECT_EQ(s[0] + s[1] + s[2], HTTP(GET(base_url + "?n=3&checked&recent=10000")).body);
  EXPECT_EQ(s[0] + s[1] + s[2], HTTP(GET(base_url + "?n=3&recent=10000")).body);

  // Test `i` + `nowait`
  // All entries from `index = 3`.
  EXPECT_EQ(s[3], HTTP(GET(base_url + "?i=3&nowait&checked")).body);
  EXPECT_EQ(s[3], HTTP(GET(base_url + "?i=3&nowait")).body);
  // All entries from `index = 1`.
  EXPECT_EQ(s[1] + s[2] + s[3], HTTP(GET(base_url + "?i=1&nowait&checked")).body);
  EXPECT_EQ(s[1] + s[2] + s[3], HTTP(GET(base_url + "?i=1&nowait")).body);

  // Test `i` + `n`
  // One entry from `index = 0`.
  EXPECT_EQ(s[0], HTTP(GET(base_url + "?i=0&n=1&checked")).body);
  EXPECT_EQ(s[0], HTTP(GET(base_url + "?i=0&n=1")).body);
  // Two entry from `index = 2`.
  EXPECT_EQ(s[2] + s[3], HTTP(GET(base_url + "?i=2&n=2&checked")).body);
  EXPECT_EQ(s[2] + s[3], HTTP(GET(base_url + "?i=2&n=2")).body);

  // Test `tail` + `nowait`
  // Last three entries.
  EXPECT_EQ(s[1] + s[2] + s[3], HTTP(GET(base_url + "?tail=3&nowait&checked")).body);
  EXPECT_EQ(s[1] + s[2] + s[3], HTTP(GET(base_url + "?tail=3&nowait")).body);
  // Large `tail` value => all entries.
  EXPECT_EQ(s[0] + s[1] + s[2] + s[3], HTTP(GET(base_url + "?tail=50&nowait&checked")).body);
  EXPECT_EQ(s[0] + s[1] + s[2] + s[3], HTTP(GET(base_url + "?tail=50&nowait")).body);

  // Test `tail` + `n`
  // One entry starting from the 4th from the end.
  EXPECT_EQ(s[0], HTTP(GET(base_url + "?tail=4&n=1&checked")).body);
  EXPECT_EQ(s[0], HTTP(GET(base_url + "?tail=4&n=1")).body);
  // Two entries starting from the 3rd from the end.
  EXPECT_EQ(s[1] + s[2], HTTP(GET(base_url + "?tail=3&n=2&checked")).body);
  EXPECT_EQ(s[1] + s[2], HTTP(GET(base_url + "?tail=3&n=2")).body);

  // Test `tail` + `i` + `nowait`.
  // More strict constraint by `tail`.
  EXPECT_EQ(s[3], HTTP(GET(base_url + "?tail=1&i=0&nowait&checked")).body);
  EXPECT_EQ(s[3], HTTP(GET(base_url + "?tail=1&i=0&nowait")).body);
  // More strict constraint by `i`.
  EXPECT_EQ(s[3], HTTP(GET(base_url + "?tail=4&i=3&nowait&checked")).body);
  EXPECT_EQ(s[3], HTTP(GET(base_url + "?tail=4&i=3&nowait")).body);
  // Nonexistent `i`.
  EXPECT_EQ("", HTTP(GET(base_url + "?tail=4&i=10&nowait&checked")).body);
  EXPECT_EQ("", HTTP(GET(base_url + "?tail=4&i=10&nowait")).body);

  // Test `tail` + `i` + `n`.
  // More strict constraint by `tail`.
  EXPECT_EQ(s[2], HTTP(GET(base_url + "?tail=2&i=0&n=1&checked")).body);
  EXPECT_EQ(s[2], HTTP(GET(base_url + "?tail=2&i=0&n=1")).body);
  // More strict constraint by `i`.
  EXPECT_EQ(s[1], HTTP(GET(base_url + "?tail=4&i=1&n=1&checked")).body);
  EXPECT_EQ(s[1], HTTP(GET(base_url + "?tail=4&i=1&n=1")).body);

  // Test `period`.
  // Start from the first entry with the `period` less than 100.
  EXPECT_EQ(s[0], HTTP(GET(base_url + "?period=99&checked")).body);
  EXPECT_EQ(s[0], HTTP(GET(base_url + "?period=99")).body);
  // Start 1 us later than the first entry with the `period` less than 200.
  EXPECT_EQ(s[1] + s[2], HTTP(GET(base_url + "?since=101&period=199&checked")).body);
  EXPECT_EQ(s[1] + s[2], HTTP(GET(base_url + "?since=101&period=199")).body);
  // Start 1 us later than the first entry with the `period` equal to 200.
  EXPECT_EQ(s[1] + s[2] + s[3], HTTP(GET(base_url + "?since=101&period=200&nowait&checked")).body);
  EXPECT_EQ(s[1] + s[2] + s[3], HTTP(GET(base_url + "?since=101&period=200&nowait")).body);
  // Start 1 us later than the first entry with the `period` equal to 200 and limit to one entry.
  EXPECT_EQ(s[1], HTTP(GET(base_url + "?since=101&period=200&n=1&checked")).body);
  EXPECT_EQ(s[1], HTTP(GET(base_url + "?since=101&period=200&n=1")).body);
  // Start from the third entry from the end with the `period` less than 100.
  EXPECT_EQ(s[1], HTTP(GET(base_url + "?tail=3&period=99&checked")).body);
  EXPECT_EQ(s[1], HTTP(GET(base_url + "?tail=3&period=99")).body);

  // Test `?stop_after_bytes=...`'
  // Request exactly the size of the first entry.
  EXPECT_EQ(s[0], HTTP(GET(base_url + "?stop_after_bytes=42&checked")).body);
  EXPECT_EQ(s[0], HTTP(GET(base_url + "?stop_after_bytes=42")).body);
  // Request slightly more max bytes than the size of the first entry.
  EXPECT_EQ(s[0] + s[1], HTTP(GET(base_url + "?stop_after_bytes=50&checked")).body);
  EXPECT_EQ(s[0] + s[1], HTTP(GET(base_url + "?stop_after_bytes=50")).body);
  // Request exactly the size of the first two entries.
  EXPECT_EQ(s[0] + s[1], HTTP(GET(base_url + "?stop_after_bytes=84&checked")).body);
  EXPECT_EQ(s[0] + s[1], HTTP(GET(base_url + "?stop_after_bytes=84")).body);
  // Request with the capacity large enough to hold all the entries.
  EXPECT_EQ(s[0] + s[1] + s[2] + s[3], HTTP(GET(base_url + "?stop_after_bytes=100000&nowait&checked")).body);
  EXPECT_EQ(s[0] + s[1] + s[2] + s[3], HTTP(GET(base_url + "?stop_after_bytes=100000&nowait")).body);

  // Test the `array` mode.
  {
    {
      const std::string body = HTTP(GET(base_url + "?n=1&array&checked")).body;
      EXPECT_EQ("[\n" + e[0] + "]\n", body);
      EXPECT_EQ(e[0], JSON(ParseJSON<std::vector<RecordWithTimestamp>>(body)[0]) + '\n');
      const std::string body_unchecked = HTTP(GET(base_url + "?n=1&array")).body;
      EXPECT_EQ("[\n" + e[0] + "]\n", body_unchecked);
      EXPECT_EQ(e[0], JSON(ParseJSON<std::vector<RecordWithTimestamp>>(body_unchecked)[0]) + '\n');
    }
    {
      const std::string body = HTTP(GET(base_url + "?n=2&array&checked")).body;
      EXPECT_EQ("[\n" + e[0] + ",\n" + e[1] + "]\n", body);
      EXPECT_EQ(e[0], JSON(ParseJSON<std::vector<RecordWithTimestamp>>(body)[0]) + '\n');
      EXPECT_EQ(e[1], JSON(ParseJSON<std::vector<RecordWithTimestamp>>(body)[1]) + '\n');
      const std::string body_unchecked = HTTP(GET(base_url + "?n=2&array")).body;
      EXPECT_EQ("[\n" + e[0] + ",\n" + e[1] + "]\n", body_unchecked);
      EXPECT_EQ(e[0], JSON(ParseJSON<std::vector<RecordWithTimestamp>>(body_unchecked)[0]) + '\n');
      EXPECT_EQ(e[1], JSON(ParseJSON<std::vector<RecordWithTimestamp>>(body_unchecked)[1]) + '\n');
    }
  }

  // TODO(dkorolev): Add tests that add data while the chunked response is in progress.
  // TODO(dkorolev): Unregister the exposed endpoint and free its handler. It's hanging out there now...
  // TODO(dkorolev): Add tests that the endpoint is not unregistered until its last client is done. (?)
}

TEST(Stream, HTTPSubscriptionCanBeTerminated) {
  current::time::ResetToZero();

  using namespace stream_unittest;

  auto exposed_stream = current::stream::Stream<Record>::CreateStream();
  const std::string base_url = Printf("http://localhost:%d/exposed", FLAGS_stream_http_test_port);

  const auto scope = HTTP(FLAGS_stream_http_test_port).Register("/exposed", *exposed_stream);

  std::atomic_bool keep_publishing(true);
  std::thread slow_publisher([&exposed_stream, &keep_publishing]() {
    int i = 1;
    while (keep_publishing) {
      exposed_stream->Publisher()->Publish(Record(i), std::chrono::microseconds(i));
      ++i;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });

  std::string subscription_id;
  std::atomic_size_t chunks_count(0);
  std::atomic_bool chunks_done(false);

  std::thread slow_subscriber([&] {
    const auto result = HTTP(ChunkedGET(base_url,
                                        [&subscription_id](const std::string& header, const std::string& value) {
                                          if (header == "X-Current-Stream-Subscription-Id") {
                                            subscription_id = value;
                                          }
                                        },
                                        [&chunks_count](const std::string& unused_chunk_body) {
                                          static_cast<void>(unused_chunk_body);
                                          ++chunks_count;
                                        },
                                        [&chunks_done]() { chunks_done = true; }));
    EXPECT_EQ(200, static_cast<int>(result));
  });

  while (chunks_count < 100) {
    std::this_thread::yield();
  }

  ASSERT_TRUE(!subscription_id.empty());
  EXPECT_EQ(64u, subscription_id.length());

  EXPECT_FALSE(chunks_done);

  {
    const auto result = HTTP(GET(base_url + "?terminate=" + subscription_id));
    EXPECT_EQ(200, static_cast<int>(result.code));
    EXPECT_EQ("", result.body);
  }

  keep_publishing = false;
  slow_publisher.join();

  while (!chunks_done) {
    std::this_thread::yield();
  }
  EXPECT_TRUE(chunks_done);

  EXPECT_GE(chunks_count, 100u);

  slow_subscriber.join();
}

const std::string golden_signature() {
  current::reflection::StructSchema struct_schema;
  struct_schema.AddType<stream_unittest::Record>();
  return "#signature " +
         JSON(current::ss::StreamSignature("StreamSchema", "TopLevelTransaction", struct_schema.GetSchemaInfo())) +
         '\n';
}

const std::string stream_golden_data = golden_signature() +
                                       "{\"index\":0,\"us\":100}\t{\"x\":1}\n"
                                       "{\"index\":1,\"us\":200}\t{\"x\":2}\n"
                                       "#head 00000000000000000300\n"
                                       "{\"index\":2,\"us\":400}\t{\"x\":3}\n"
                                       "#head 00000000000000000500\n";

const std::string stream_golden_data_single_head = golden_signature() +
                                                   "{\"index\":0,\"us\":100}\t{\"x\":1}\n"
                                                   "{\"index\":1,\"us\":200}\t{\"x\":2}\n"
                                                   "{\"index\":2,\"us\":400}\t{\"x\":3}\n"
                                                   "#head 00000000000000000500\n";

// clang-format off
const std::string stream_golden_data_chunks[] = {
  "{\"index\":0,\"u","s\":100}\t{\"x\":1}\r","\n"
  "{\"index\":1,\"us\":200}\t{\"x\":2}\n\r\n"
  "{\"us\"",":3","00}\n"
  "{\"index\":2,\"us\":400}\t{\"x","\":3}\n"
  "{\"us\":500}\n"
};
// clang-format on

TEST(Stream, PersistsToFile) {
  current::time::ResetToZero();

  using namespace stream_unittest;

  const std::string persistence_file_name = current::FileSystem::JoinPath(FLAGS_stream_test_tmpdir, "data");
  const auto persistence_file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

  auto persisted = current::stream::Stream<Record, current::persistence::File>::CreateStream(persistence_file_name);

  current::time::SetNow(std::chrono::microseconds(100));
  persisted->Publisher()->Publish(Record(1));
  current::time::SetNow(std::chrono::microseconds(200));
  persisted->Publisher()->Publish(Record(2));
  current::time::SetNow(std::chrono::microseconds(300));
  persisted->Publisher()->UpdateHead();
  current::time::SetNow(std::chrono::microseconds(400));
  persisted->Publisher()->Publish(Record(3));
  current::time::SetNow(std::chrono::microseconds(450));
  persisted->Publisher()->UpdateHead();
  current::time::SetNow(std::chrono::microseconds(500));
  persisted->Publisher()->UpdateHead();

  // This spin lock is unnecessary as publishing is synchronous as of now. -- D.K.
  while (current::FileSystem::GetFileSize(persistence_file_name) < stream_golden_data.size()) {
    std::this_thread::yield();
  }

  EXPECT_EQ(stream_golden_data, current::FileSystem::ReadFileAsString(persistence_file_name));
}

TEST(Stream, ParsesFromFile) {
  current::time::ResetToZero();

  using namespace stream_unittest;

  const std::string persistence_file_name = current::FileSystem::JoinPath(FLAGS_stream_test_tmpdir, "data");
  const auto persistence_file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);
  current::FileSystem::WriteStringToFile(stream_golden_data, persistence_file_name.c_str());

  auto parsed = current::stream::Stream<Record, current::persistence::File>::CreateStream(persistence_file_name);

  Data d;
  Data d_unchecked;
  {
    ASSERT_FALSE(d.subscriber_alive_);
    ASSERT_FALSE(d_unchecked.subscriber_alive_);
    StreamTestProcessor p(d, false, true);
    StreamTestProcessor p_unchecked(d_unchecked, false, true);
    ASSERT_TRUE(d.subscriber_alive_);
    ASSERT_TRUE(d_unchecked.subscriber_alive_);
    p.SetMax(4u);
    p_unchecked.SetMax(4u);
    parsed->Subscribe(p);  // A blocking call until the subscriber processes three entries and one head update.
    parsed->SubscribeUnchecked(p_unchecked);
    EXPECT_EQ(4u, d.seen_);
    EXPECT_EQ(500, d.head_.count());
    ASSERT_TRUE(d.subscriber_alive_);
    EXPECT_EQ(4u, d_unchecked.seen_);
    EXPECT_EQ(500, d_unchecked.head_.count());
    ASSERT_TRUE(d_unchecked.subscriber_alive_);
  }
  ASSERT_FALSE(d.subscriber_alive_);
  ASSERT_FALSE(d_unchecked.subscriber_alive_);

  {
    // Try scope-based subscription of a limited-range subscriber, and confirm
    // casting the scope of this subscription to `bool` eventially become `false`.
    Data d2;
    Data d2_unchecked;
    {
      ASSERT_FALSE(d2.subscriber_alive_);
      ASSERT_FALSE(d2_unchecked.subscriber_alive_);
      StreamTestProcessor p2(d2, false, true);
      StreamTestProcessor p2_unchecked(d2_unchecked, false, true);
      ASSERT_TRUE(d2.subscriber_alive_);
      ASSERT_TRUE(d2_unchecked.subscriber_alive_);
      p2.SetMax(4u);
      p2_unchecked.SetMax(4u);
      const auto scope = parsed->Subscribe(p2);
      while (static_cast<bool>(scope)) {
        std::this_thread::yield();
      }
      EXPECT_FALSE(static_cast<bool>(scope));
      const auto scope_unchecked = parsed->SubscribeUnchecked(p2_unchecked);
      while (static_cast<bool>(scope_unchecked)) {
        std::this_thread::yield();
      }
      EXPECT_FALSE(static_cast<bool>(scope_unchecked));
      EXPECT_EQ(4u, d2.seen_);
      EXPECT_EQ(500, d2.head_.count());
      EXPECT_TRUE(d2.subscriber_alive_);
      EXPECT_EQ(4u, d2_unchecked.seen_);
      EXPECT_EQ(500, d2_unchecked.head_.count());
      EXPECT_TRUE(d2_unchecked.subscriber_alive_);
    }
    EXPECT_FALSE(d2.subscriber_alive_);
    EXPECT_FALSE(d2_unchecked.subscriber_alive_);
  }

  const std::vector<std::string> expected_values{"[0:100,2:400] 1", "[1:200,2:400] 2", "[2:400,2:400] 3"};
  const auto joined_expected_values = Join(expected_values, ',');
  // A careful condition, since the subscriber may process some or all entries before going out of scope.
  EXPECT_TRUE(CompareValuesMixedWithTerminate(d.results_, expected_values, StreamTestProcessor::kTerminateStr))
      << joined_expected_values << " != " << d.results_;
  EXPECT_TRUE(
      CompareValuesMixedWithTerminate(d_unchecked.results_, expected_values, StreamTestProcessor::kTerminateStr))
      << joined_expected_values << " != " << d_unchecked.results_;
}

TEST(Stream, UncheckedVsCheckedSubscription) {
  using namespace stream_unittest;

  const std::string persistence_file_name = current::FileSystem::JoinPath(FLAGS_stream_test_tmpdir, "data");
  const auto persistence_file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

  const std::string signature = golden_signature();
  std::vector<std::string> lines = {"{\"index\":0,\"us\":100}\t{\"x\":1}",
                                    "{\"index\":1,\"us\":200}\t{\"x\":2}",
                                    "#head 00000000000000000300",
                                    "{\"index\":2,\"us\":400}\t{\"x\":3}",
                                    "#head 00000000000000000500"};
  const auto CombineFileContents = [&]() -> std::string {
    std::string contents = signature;
    for (const auto& line : lines) {
      contents += line + '\n';
    }
    return contents;
  };

  current::FileSystem::WriteStringToFile(CombineFileContents(), persistence_file_name.c_str());
  auto exposed_stream =
      current::stream::Stream<Record, current::persistence::File>::CreateStream(persistence_file_name);

  const std::string base_url = Printf("http://localhost:%d/exposed", FLAGS_stream_http_test_port);
  const auto scope = HTTP(FLAGS_stream_http_test_port).Register("/exposed", *exposed_stream);

  const auto CollectSubscriptionResult = [&](std::vector<std::string>& rows, std::vector<std::string>& entries) {
    rows.clear();
    entries.clear();
    RecordsCollector collector(entries, rows);
    const auto scope = exposed_stream->Subscribe(collector);
    while (collector.count_ < 3u) {
      std::this_thread::yield();
    }
  };
  const auto CollectUncheckedSubscriptionResult =
      [&](std::vector<std::string>& rows, std::vector<std::string>& entries) {
        rows.clear();
        entries.clear();
        RecordsUncheckedCollector collector(entries, rows);
        const auto scope = exposed_stream->SubscribeUnchecked(collector);
        while (collector.count_ < 3u) {
          std::this_thread::yield();
        }
      };

  {
    std::vector<std::string> all_entries;
    std::vector<std::string> all_rows;
    const auto expected_rows =
        "{\"index\":0,\"us\":100}\t{\"x\":1}\n"
        "{\"index\":1,\"us\":200}\t{\"x\":2}\n"
        "{\"index\":2,\"us\":400}\t{\"x\":3}\n";
    const auto expected_entries =
        "{\"x\":1}\n"
        "{\"x\":2}\n"
        "{\"x\":3}\n";

    CollectSubscriptionResult(all_entries, all_rows);
    EXPECT_EQ(expected_rows, Join(all_rows, ""));
    EXPECT_EQ(expected_entries, Join(all_entries, ""));

    CollectUncheckedSubscriptionResult(all_entries, all_rows);
    EXPECT_EQ(expected_rows, Join(all_rows, ""));
    EXPECT_EQ(expected_entries, Join(all_entries, ""));

    EXPECT_EQ(expected_rows, HTTP(GET(base_url + "?nowait&checked")).body);
    EXPECT_EQ(expected_rows, HTTP(GET(base_url + "?nowait")).body);
    EXPECT_EQ(expected_entries, HTTP(GET(base_url + "?nowait&entries_only&checked")).body);
    EXPECT_EQ(expected_entries, HTTP(GET(base_url + "?nowait&entries_only")).body);
  }

  {
    // Swap the first two entries, which should provoke the `InconsistentIndexException` exceptions
    // in case of the "Checked" subscription, but for the "Unchecked" it should't.
    std::swap(lines[0], lines[1]);
    current::FileSystem::WriteStringToFile(CombineFileContents(), persistence_file_name.c_str());

    std::vector<std::string> entries;
    std::vector<std::string> rows;
    const auto expected_rows =
        "{\"index\":1,\"us\":200}\t{\"x\":2}\n"
        "{\"index\":0,\"us\":100}\t{\"x\":1}\n"
        "{\"index\":2,\"us\":400}\t{\"x\":3}\n";
    const auto expected_entries =
        "{\"x\":2}\n"
        "{\"x\":1}\n"
        "{\"x\":3}\n";

    EXPECT_NO_THROW(CollectUncheckedSubscriptionResult(entries, rows));
    EXPECT_EQ(expected_rows, Join(rows, ""));
    EXPECT_EQ(expected_entries, Join(entries, ""));
    EXPECT_EQ(expected_rows, HTTP(GET(base_url + "?nowait")).body);
    EXPECT_EQ(expected_entries, HTTP(GET(base_url + "?nowait&entries_only")).body);
    // Restore the correct lines ordering before the next test case.
    std::swap(lines[0], lines[1]);
  }

  {
    // Produce wrong JSON instead of an entire entry - replace both index and value with garbage.
    lines[1] = std::string(lines[1].length(), 'A');
    current::FileSystem::WriteStringToFile(CombineFileContents(), persistence_file_name.c_str());

    std::vector<std::string> entries;
    std::vector<std::string> rows;
    const auto expected_rows =
        "{\"index\":0,\"us\":100}\t{\"x\":1}\n"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
        "{\"index\":2,\"us\":400}\t{\"x\":3}\n";

    EXPECT_NO_THROW(CollectUncheckedSubscriptionResult(entries, rows));
    EXPECT_EQ(expected_rows, Join(rows, ""));
    EXPECT_EQ(
        "{\"x\":1}\n"
        "Malformed row\n"
        "{\"x\":3}\n",
        Join(entries, ""));
    EXPECT_EQ(expected_rows, HTTP(GET(base_url + "?nowait")).body);
    EXPECT_EQ(
        "{\"x\":1}\n"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
        "{\"x\":3}\n",
        HTTP(GET(base_url + "?nowait&entries_only")).body);
    EXPECT_EQ("AAAAAAAAAAAAAAAAAAAAAAAAAAAA\n", HTTP(GET(base_url + "?i=1&n=1&nowait")).body);
    EXPECT_EQ("AAAAAAAAAAAAAAAAAAAAAAAAAAAA\n", HTTP(GET(base_url + "?i=1&n=1&nowait&entries_only")).body);
    EXPECT_EQ("{\"index\":2,\"us\":400}\t{\"x\":3}\n", HTTP(GET(base_url + "?i=2&n=1&nowait")).body);
    EXPECT_EQ("{\"x\":3}\n", HTTP(GET(base_url + "?i=2&n=1&nowait&entries_only")).body);
  }

  {
    // Produce wrong JSON instead of the index and timestamp only and leave the entry value valid.
    const auto tab_pos = lines[0].find('\t');
    ASSERT_FALSE(std::string::npos == tab_pos);
    lines[0].replace(0, tab_pos, tab_pos, 'B');
    current::FileSystem::WriteStringToFile(CombineFileContents(), persistence_file_name.c_str());

    std::vector<std::string> entries;
    std::vector<std::string> rows;
    const auto expected_rows =
        "BBBBBBBBBBBBBBBBBBBB\t{\"x\":1}\n"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
        "{\"index\":2,\"us\":400}\t{\"x\":3}\n";

    EXPECT_NO_THROW(CollectUncheckedSubscriptionResult(entries, rows));
    EXPECT_EQ(expected_rows, Join(rows, ""));
    EXPECT_EQ(
        "Malformed index and timestamp\n"
        "Malformed row\n"
        "{\"x\":3}\n",
        Join(entries, ""));
    EXPECT_EQ(expected_rows, HTTP(GET(base_url + "?nowait")).body);
    EXPECT_EQ(
        "{\"x\":1}\n"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
        "{\"x\":3}\n",
        HTTP(GET(base_url + "?nowait&entries_only")).body);
    EXPECT_EQ("BBBBBBBBBBBBBBBBBBBB\t{\"x\":1}\n", HTTP(GET(base_url + "?i=0&n=1&nowait")).body);
    EXPECT_EQ("{\"x\":1}\n", HTTP(GET(base_url + "?i=0&n=1&nowait&entries_only")).body);
  }

  {
    // Produce wrong JSON instead of the entry's value only and leave index and timestamp section valid.
    const auto tab_pos = lines[3].find('\t');
    ASSERT_FALSE(std::string::npos == tab_pos);
    const auto length_to_replace = lines[3].length() - (tab_pos + 1);
    lines[3].replace(tab_pos + 1, length_to_replace, length_to_replace, 'C');
    current::FileSystem::WriteStringToFile(CombineFileContents(), persistence_file_name.c_str());

    std::vector<std::string> entries;
    std::vector<std::string> rows;
    const auto expected_rows =
        "BBBBBBBBBBBBBBBBBBBB\t{\"x\":1}\n"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
        "{\"index\":2,\"us\":400}\tCCCCCCC\n";

    EXPECT_NO_THROW(CollectUncheckedSubscriptionResult(entries, rows));
    EXPECT_EQ(expected_rows, Join(rows, ""));
    EXPECT_EQ(
        "Malformed index and timestamp\n"
        "Malformed row\n"
        "Malformed entry\n",
        Join(entries, ""));
    EXPECT_EQ(expected_rows, HTTP(GET(base_url + "?nowait")).body);
    EXPECT_EQ(
        "{\"x\":1}\n"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
        "CCCCCCC\n",
        HTTP(GET(base_url + "?nowait&entries_only")).body);
    EXPECT_EQ("{\"index\":2,\"us\":400}\tCCCCCCC\n", HTTP(GET(base_url + "?i=2&n=1&nowait")).body);
    EXPECT_EQ("CCCCCCC\n", HTTP(GET(base_url + "?i=2&n=1&nowait&entries_only")).body);
  }
}

TEST(Stream, ParseArbitrarilySplitChunks) {
  using namespace stream_unittest;

  const std::string persistence_file_name = current::FileSystem::JoinPath(FLAGS_stream_test_tmpdir, "data");
  const auto persistence_file_remover = current::FileSystem::ScopedRmFile(persistence_file_name);

  using stream_t = current::stream::Stream<Record, current::persistence::File>;
  using RemoteStreamReplicator = current::stream::StreamReplicator<stream_t>;
  auto replicated_stream = stream_t::CreateStream(persistence_file_name);

  // Simulate subscription to stream stream.
  const auto scope =
      HTTP(FLAGS_stream_http_test_port)
          .Register("/log",
                    URLPathArgs::CountMask::None | URLPathArgs::CountMask::One,
                    [](Request r) {
                      EXPECT_EQ("GET", r.method);
                      const std::string subscription_id = "fake_subscription";
                      if (r.url.query.has("terminate")) {
                        EXPECT_EQ(r.url.query["terminate"], subscription_id);
                        r("", HTTPResponseCode.OK);
                      } else if (r.url.query.has("i")) {
                        const auto index = current::FromString<uint64_t>(r.url.query["i"]);
                        auto response = r.connection.SendChunkedHTTPResponse(
                            HTTPResponseCode.OK,
                            current::net::http::Headers({{"X-Current-Stream-Subscription-Id", subscription_id}}),
                            "text/plain");
                        if (index == 0u) {
                          for (const auto& chunk : stream_golden_data_chunks) {
                            response.Send(chunk);
                          }
                        } else {
                          EXPECT_EQ(3u, index);
                        }
                      } else {
                        EXPECT_EQ(1u, r.url_path_args.size());
                        EXPECT_EQ("schema.simple", r.url_path_args[0]);
                        r(current::stream::SubscribableStreamSchema(
                            Value<current::reflection::ReflectedTypeBase>(
                                current::reflection::Reflector().ReflectType<Record>()).type_id,
                            "Record",
                            "Namespace"));
                      }
                    });

  // Replicate data via subscription to the fake stream.
  current::stream::SubscribableRemoteStream<Record> remote_stream(
      Printf("http://localhost:%d/log", FLAGS_stream_http_test_port), "Record", "Namespace");

  auto replicator = RemoteStreamReplicator(replicated_stream);

  {
    const auto subscriber_scope = remote_stream.Subscribe(replicator);
    while (replicated_stream->Data()->Size() < 3u) {
      std::this_thread::yield();
    }
  }

  EXPECT_EQ(stream_golden_data, current::FileSystem::ReadFileAsString(persistence_file_name));
}

TEST(Stream, MasterFollowerFlip) {
  current::time::ResetToZero();

  using namespace stream_unittest;
  using stream_t = current::stream::Stream<Record, current::persistence::File>;

  const std::string stream1_file_name = current::FileSystem::JoinPath(FLAGS_stream_test_tmpdir, "stream1");
  const std::string stream2_file_name = current::FileSystem::JoinPath(FLAGS_stream_test_tmpdir, "stream2");
  const std::string stream3_file_name = current::FileSystem::JoinPath(FLAGS_stream_test_tmpdir, "stream3");
  const auto stream1_file_remover = current::FileSystem::ScopedRmFile(stream1_file_name);
  const auto stream2_file_remover = current::FileSystem::ScopedRmFile(stream2_file_name);
  const auto stream3_file_remover = current::FileSystem::ScopedRmFile(stream3_file_name);
  const auto port1 = FLAGS_stream_http_test_port;
  const auto port2 = FLAGS_stream_http_test_port + 1;
  const std::string base_url1 = Printf("http://localhost:%d/exposed", port1);
  const std::string base_url2 = Printf("http://localhost:%d/exposed", port2);

  current::FileSystem::WriteStringToFile(stream_golden_data, stream1_file_name.c_str());
  current::stream::MasterFlipController<stream_t> stream1(stream_t::CreateStream(stream1_file_name));
  auto flip_key1 = stream1.ExposeViaHTTP(port1, "/exposed");
  auto stream2_separate = stream_t::CreateStream(stream2_file_name);
  current::stream::MasterFlipController<stream_t> stream2(Value(stream2_separate));
  stream2.FollowRemoteStream(base_url1, current::stream::SubscriptionMode::Checked);
  EXPECT_FALSE(stream2.IsMasterStream());
  EXPECT_TRUE(stream1.IsMasterStream());

  stream2.FlipToMaster(flip_key1);
  EXPECT_TRUE(stream2.IsMasterStream());
  EXPECT_FALSE(stream1.IsMasterStream());

  const auto flip_key2 = stream2.ExposeViaHTTP(port2, "/exposed");
  stream1.FollowRemoteStream(base_url2);
  current::stream::MasterFlipController<stream_t> stream3(stream_t::CreateStream(stream3_file_name));
  stream3.FollowRemoteStream(base_url1);
  EXPECT_FALSE(stream1.IsMasterStream());
  EXPECT_TRUE(stream2.IsMasterStream());
  EXPECT_FALSE(stream3.IsMasterStream());

  stream3.FlipToMaster(flip_key2);
  EXPECT_FALSE(stream1.IsMasterStream());
  EXPECT_FALSE(stream2.IsMasterStream());
  EXPECT_TRUE(stream3.IsMasterStream());

  EXPECT_EQ(stream_golden_data_single_head, current::FileSystem::ReadFileAsString(stream2_file_name));
  EXPECT_EQ(stream_golden_data_single_head, current::FileSystem::ReadFileAsString(stream3_file_name));
}

TEST(Stream, MasterFollowerFlipRestrictions) {
  current::time::ResetToZero();

  using namespace stream_unittest;
  using stream_t = current::stream::Stream<Record, current::persistence::File>;

  const std::string stream1_file_name = current::FileSystem::JoinPath(FLAGS_stream_test_tmpdir, "stream1");
  const std::string stream2_file_name = current::FileSystem::JoinPath(FLAGS_stream_test_tmpdir, "stream2");
  const auto stream1_file_remover = current::FileSystem::ScopedRmFile(stream1_file_name);
  const auto port1 = FLAGS_stream_http_test_port;
  const auto port2 = FLAGS_stream_http_test_port + 1;
  const std::string base_url1 = Printf("http://localhost:%d/exposed", port1);
  const std::string base_url2 = Printf("http://localhost:%d/exposed", port2);

  struct DummyReplicatorImpl {
    using EntryResponse = current::ss::EntryResponse;
    using TerminationResponse = current::ss::TerminationResponse;
    using entry_t = typename stream_t::entry_t;

    virtual ~DummyReplicatorImpl() {}

    EntryResponse operator()(entry_t&&, idxts_t, idxts_t) { return EntryResponse::More; }
    EntryResponse operator()(const entry_t&, idxts_t, idxts_t) { return EntryResponse::More; }
    EntryResponse operator()(std::string&&, uint64_t, idxts_t) { return EntryResponse::More; }
    EntryResponse operator()(const std::string&, uint64_t, idxts_t) { return EntryResponse::More; }
    EntryResponse operator()(std::chrono::microseconds) { return EntryResponse::More; }
    EntryResponse EntryResponseIfNoMorePassTypeFilter() const { return EntryResponse::More; }
    TerminationResponse Terminate() const { return TerminationResponse::Terminate; }
  };
  using DummyReplicator = current::ss::StreamSubscriber<DummyReplicatorImpl, Record>;
  DummyReplicator dummy_replicator;
  current::stream::MasterFlipController<stream_t> stream1(stream_t::CreateStream(stream1_file_name));
  current::time::SetNow(std::chrono::microseconds(10));
  stream1->Publisher()->Publish(Record(1));
  current::time::SetNow(std::chrono::microseconds(20));
  stream1->Publisher()->Publish(Record(2));

  // Permanent restrictions.
  {
    auto head_idxts = stream1->Data()->HeadAndLastPublishedIndexAndTimestamp();
    auto flip_key = stream1.ExposeViaHTTP(port1, "/exposed");
    current::stream::SubscribableRemoteStream<Record> remote_stream(base_url1);

    // Restriction 1: The head of the follower shouldn't be ahead of the master.
    head_idxts.head += std::chrono::microseconds(1);
    ASSERT_THROW((remote_stream.template FlipToMaster<DummyReplicator, current::stream::ReplicationMode::Unchecked>(
                     dummy_replicator, head_idxts, flip_key, current::stream::SubscriptionMode::Unchecked)),
                 current::stream::RemoteStreamRefusedFlipRequestException);
    ASSERT_THROW((remote_stream.template FlipToMaster<DummyReplicator, current::stream::ReplicationMode::Checked>(
                     dummy_replicator, head_idxts, flip_key, current::stream::SubscriptionMode::Checked)),
                 current::stream::RemoteStreamRefusedFlipRequestException);

    // Restriction 2: The last index of the follower shouldn't be greater than the one of the master.
    head_idxts = stream1->Data()->HeadAndLastPublishedIndexAndTimestamp();
    ++Value(head_idxts.idxts).index;
    ASSERT_THROW((remote_stream.template FlipToMaster<DummyReplicator, current::stream::ReplicationMode::Unchecked>(
                     dummy_replicator, head_idxts, flip_key, current::stream::SubscriptionMode::Unchecked)),
                 current::stream::RemoteStreamRefusedFlipRequestException);
    ASSERT_THROW((remote_stream.template FlipToMaster<DummyReplicator, current::stream::ReplicationMode::Checked>(
                     dummy_replicator, head_idxts, flip_key, current::stream::SubscriptionMode::Checked)),
                 current::stream::RemoteStreamRefusedFlipRequestException);

    // Restriction 3: The head of the follower should be less than the timestamp of the next entry.
    head_idxts = stream1->Data()->HeadAndLastPublishedIndexAndTimestamp();
    --Value(head_idxts.idxts).index;
    ASSERT_THROW((remote_stream.template FlipToMaster<DummyReplicator, current::stream::ReplicationMode::Unchecked>(
                     dummy_replicator, head_idxts, flip_key, current::stream::SubscriptionMode::Unchecked)),
                 current::stream::RemoteStreamRefusedFlipRequestException);
    ASSERT_THROW((remote_stream.template FlipToMaster<DummyReplicator, current::stream::ReplicationMode::Checked>(
                     dummy_replicator, head_idxts, flip_key, current::stream::SubscriptionMode::Checked)),
                 current::stream::RemoteStreamRefusedFlipRequestException);

    // Restriction 4: The head of the follower should be greater or equal than the timestamp of the last entry.
    head_idxts = stream1->Data()->HeadAndLastPublishedIndexAndTimestamp();
    Value(head_idxts.idxts).us -= std::chrono::microseconds(10);
    head_idxts.head -= std::chrono::microseconds(10);
    ASSERT_THROW((remote_stream.template FlipToMaster<DummyReplicator, current::stream::ReplicationMode::Unchecked>(
                     dummy_replicator, head_idxts, flip_key, current::stream::SubscriptionMode::Unchecked)),
                 current::stream::RemoteStreamRefusedFlipRequestException);
    ASSERT_THROW((remote_stream.template FlipToMaster<DummyReplicator, current::stream::ReplicationMode::Checked>(
                     dummy_replicator, head_idxts, flip_key, current::stream::SubscriptionMode::Checked)),
                 current::stream::RemoteStreamRefusedFlipRequestException);

    stream1.StopExposingViaHTTP();
  }

  std::atomic<bool> flip_started_called;
  std::atomic<bool> flip_finished_called;
  std::atomic<bool> flip_canceled_called;
  const auto WaitForFlipCompletion = [&]() {
    // If, for some reason, the flip hasn't already started, the rest of the callbacks
    // will not be called, so there is no point to wait for any of them.
    if (flip_started_called) {
      while (!flip_finished_called && !flip_canceled_called) {
        std::this_thread::yield();
      }
    }
  };

  // Custom restriction 1: the difference between last indices of the master and the follower.
  {
    const auto stream2_file_remover = current::FileSystem::ScopedRmFile(stream2_file_name);
    current::stream::MasterFlipController<stream_t> stream2(stream_t::CreateStream(stream2_file_name));

    auto flip_key = stream1.ExposeViaHTTP(port1,
                                          "/exposed",
                                          current::stream::MasterFlipRestrictions().SetMaxIndexDifference(1),
                                          // A little trick: we can publish from the `flip_started_callback`,
                                          // the stream is still master there.
                                          [&]() {
                                            flip_started_called = true;
                                            if (stream1->Data()->Size() < 3) {
                                              current::time::SetNow(std::chrono::microseconds(30));
                                              stream1->Publisher()->Publish(Record(3));
                                              current::time::SetNow(std::chrono::microseconds(40));
                                              stream1->Publisher()->Publish(Record(4));
                                            }
                                          },
                                          [&]() { flip_finished_called = true; },
                                          [&]() { flip_canceled_called = true; });
    stream2.FollowRemoteStream(base_url1, current::stream::SubscriptionMode::Checked);
    while (stream2->Data()->Size() != stream1->Data()->Size()) {
      std::this_thread::yield();
    }
    EXPECT_EQ(2u, stream2->Data()->Size());
    flip_started_called = flip_finished_called = flip_canceled_called = false;
    ASSERT_THROW(stream2.FlipToMaster(flip_key), current::stream::RemoteStreamRefusedFlipRequestException);
    WaitForFlipCompletion();
    EXPECT_TRUE(flip_started_called);
    EXPECT_FALSE(flip_finished_called);
    EXPECT_TRUE(flip_canceled_called);
    while (stream2->Data()->Size() != stream1->Data()->Size()) {
      std::this_thread::yield();
    }
    EXPECT_EQ(4u, stream2->Data()->Size());
    flip_started_called = flip_finished_called = flip_canceled_called = false;
    stream2.FlipToMaster(flip_key);
    WaitForFlipCompletion();
    EXPECT_TRUE(flip_started_called);
    EXPECT_TRUE(flip_finished_called);
    EXPECT_FALSE(flip_canceled_called);
    flip_key = stream2.ExposeViaHTTP(port2, "/exposed");
    stream1.FollowRemoteStream(base_url2, current::stream::SubscriptionMode::Checked);
    stream1.FlipToMaster(flip_key);
    stream1.StopExposingViaHTTP();
  }

  // Custom restriction 2: the difference between heads of the master and the follower.
  {
    const auto stream2_file_remover = current::FileSystem::ScopedRmFile(stream2_file_name);
    current::stream::MasterFlipController<stream_t> stream2(stream_t::CreateStream(stream2_file_name));

    auto flip_key = stream1.ExposeViaHTTP(
        port1,
        "/exposed",
        current::stream::MasterFlipRestrictions().SetMaxHeadDifference(std::chrono::microseconds(20)),
        [&]() {
          flip_started_called = true;
          if (stream1->Data()->CurrentHead() < std::chrono::microseconds(70)) {
            current::time::SetNow(std::chrono::microseconds(70));
            stream1->Publisher()->UpdateHead();
          }
        },
        [&]() { flip_finished_called = true; },
        [&]() { flip_canceled_called = true; });
    stream2.FollowRemoteStream(base_url1, current::stream::SubscriptionMode::Checked);
    current::time::SetNow(std::chrono::microseconds(49));
    stream1->Publisher()->UpdateHead();
    while (stream2->Data()->CurrentHead() != stream1->Data()->CurrentHead()) {
      std::this_thread::yield();
    }
    EXPECT_EQ(4u, stream2->Data()->Size());
    EXPECT_EQ(std::chrono::microseconds(49), stream2->Data()->CurrentHead());
    flip_started_called = flip_finished_called = flip_canceled_called = false;
    ASSERT_THROW(stream2.FlipToMaster(flip_key), current::stream::RemoteStreamRefusedFlipRequestException);
    WaitForFlipCompletion();
    EXPECT_TRUE(flip_started_called);
    EXPECT_FALSE(flip_finished_called);
    EXPECT_TRUE(flip_canceled_called);
    while (stream2->Data()->CurrentHead() != stream1->Data()->CurrentHead()) {
      std::this_thread::yield();
    }
    EXPECT_EQ(4u, stream2->Data()->Size());
    EXPECT_EQ(std::chrono::microseconds(70), stream2->Data()->CurrentHead());
    flip_started_called = flip_finished_called = flip_canceled_called = false;
    stream2.FlipToMaster(flip_key);
    WaitForFlipCompletion();
    EXPECT_TRUE(flip_started_called);
    EXPECT_TRUE(flip_finished_called);
    EXPECT_FALSE(flip_canceled_called);
    flip_key = stream2.ExposeViaHTTP(port2, "/exposed");
    stream1.FollowRemoteStream(base_url2, current::stream::SubscriptionMode::Checked);
    stream1.FlipToMaster(flip_key);
    stream1.StopExposingViaHTTP();
  }

  // Custom restriction 3: maximum diff size in bytes between the master and the follower.
  {
    const auto stream2_file_remover = current::FileSystem::ScopedRmFile(stream2_file_name);
    current::stream::MasterFlipController<stream_t> stream2(stream_t::CreateStream(stream2_file_name));

    // Calculate the next entry size (2 stays for \t and \n) to make the restriction just 1 byte smaller.
    const auto entry_size = JSON(Record(55)).length() + JSON(idxts_t(4, std::chrono::microseconds(80))).length() + 2;

    auto flip_key = stream1.ExposeViaHTTP(port1,
                                          "/exposed",
                                          current::stream::MasterFlipRestrictions().SetMaxDiffSize(entry_size - 1),
                                          [&]() {
                                            flip_started_called = true;
                                            if (stream1->Data()->CurrentHead() < std::chrono::microseconds(80)) {
                                              current::time::SetNow(std::chrono::microseconds(80));
                                              stream1->Publisher()->Publish(Record(55));
                                            } else if (stream1->Data()->CurrentHead() < std::chrono::microseconds(90)) {
                                              current::time::SetNow(std::chrono::microseconds(90));
                                              stream1->Publisher()->Publish(Record(6));
                                            }
                                          },
                                          [&]() { flip_finished_called = true; },
                                          [&]() { flip_canceled_called = true; });
    stream2.FollowRemoteStream(base_url1, current::stream::SubscriptionMode::Checked);
    while (stream2->Data()->CurrentHead() != stream1->Data()->CurrentHead()) {
      std::this_thread::yield();
    }
    EXPECT_EQ(4u, stream2->Data()->Size());
    EXPECT_EQ(std::chrono::microseconds(70), stream2->Data()->CurrentHead());
    flip_started_called = flip_finished_called = flip_canceled_called = false;
    ASSERT_THROW(stream2.FlipToMaster(flip_key), current::stream::RemoteStreamRefusedFlipRequestException);
    WaitForFlipCompletion();
    EXPECT_TRUE(flip_started_called);
    EXPECT_FALSE(flip_finished_called);
    EXPECT_TRUE(flip_canceled_called);
    while (stream2->Data()->CurrentHead() != stream1->Data()->CurrentHead()) {
      std::this_thread::yield();
    }
    EXPECT_EQ(5u, stream2->Data()->Size());
    EXPECT_EQ(std::chrono::microseconds(80), stream2->Data()->CurrentHead());
    flip_started_called = flip_finished_called = flip_canceled_called = false;
    stream2.FlipToMaster(flip_key);
    EXPECT_EQ(6u, stream2->Data()->Size());
    EXPECT_EQ(std::chrono::microseconds(90), stream2->Data()->CurrentHead());
    WaitForFlipCompletion();
    EXPECT_TRUE(flip_started_called);
    EXPECT_TRUE(flip_finished_called);
    EXPECT_FALSE(flip_canceled_called);
    flip_key = stream2.ExposeViaHTTP(port2, "/exposed");
    stream1.FollowRemoteStream(base_url2, current::stream::SubscriptionMode::Checked);
    stream1.FlipToMaster(flip_key);
    stream1.StopExposingViaHTTP();
  }

  // Custom restriction 4: maximum clock difference between the master and the follower.
  {
    const auto head_idxts = stream1->Data()->HeadAndLastPublishedIndexAndTimestamp();
    const auto max_clock_diff = std::chrono::microseconds(10);

    const auto flip_key =
        stream1.ExposeViaHTTP(port1,
                              "/exposed",
                              current::stream::MasterFlipRestrictions().SetMaxClockDifference(max_clock_diff),
                              [&]() { flip_started_called = true; },
                              [&]() { flip_finished_called = true; },
                              [&]() { flip_canceled_called = true; });
    current::stream::SubscribableRemoteStream<Record>::RemoteStream remote_stream(
        base_url1, current::stream::constants::kDefaultTopLevelName, current::stream::constants::kDefaultNamespaceName);
    const auto url_checked =
        remote_stream.GetFlipToMasterURL(head_idxts, flip_key, current::stream::SubscriptionMode::Checked);
    const auto url_unchecked =
        remote_stream.GetFlipToMasterURL(head_idxts, flip_key, current::stream::SubscriptionMode::Checked);
    current::time::SetNow(current::time::Now() + max_clock_diff + std::chrono::microseconds(1));

    flip_started_called = flip_finished_called = flip_canceled_called = false;
    EXPECT_EQ(400, static_cast<int>(HTTP(GET(url_checked)).code));
    WaitForFlipCompletion();
    EXPECT_FALSE(flip_started_called);
    EXPECT_FALSE(flip_finished_called);
    EXPECT_FALSE(flip_canceled_called);
    EXPECT_EQ(400, static_cast<int>(HTTP(GET(url_unchecked)).code));
    WaitForFlipCompletion();
    EXPECT_FALSE(flip_started_called);
    EXPECT_FALSE(flip_finished_called);
    EXPECT_FALSE(flip_canceled_called);

    const auto url2 =
        remote_stream.GetFlipToMasterURL(head_idxts, flip_key, current::stream::SubscriptionMode::Checked);
    current::time::SetNow(current::time::Now() + max_clock_diff);
    const auto response = HTTP(GET(url2));
    EXPECT_EQ(200, static_cast<int>(response.code));
    EXPECT_EQ("", response.body);
    WaitForFlipCompletion();
    EXPECT_TRUE(flip_started_called);
    EXPECT_TRUE(flip_finished_called);
    EXPECT_FALSE(flip_canceled_called);
  }
}

TEST(Stream, MasterFollowerFlipExceptions) {
  current::time::ResetToZero();

  using namespace stream_unittest;
  using stream_t = current::stream::Stream<Record, current::persistence::File>;

  const std::string stream1_file_name = current::FileSystem::JoinPath(FLAGS_stream_test_tmpdir, "stream1");
  const std::string stream2_file_name = current::FileSystem::JoinPath(FLAGS_stream_test_tmpdir, "stream2");
  const std::string stream3_file_name = current::FileSystem::JoinPath(FLAGS_stream_test_tmpdir, "stream3");
  const auto stream1_file_remover = current::FileSystem::ScopedRmFile(stream1_file_name);
  const auto stream2_file_remover = current::FileSystem::ScopedRmFile(stream2_file_name);
  const auto stream3_file_remover = current::FileSystem::ScopedRmFile(stream3_file_name);
  const auto port1 = FLAGS_stream_http_test_port;
  const auto port2 = FLAGS_stream_http_test_port + 1;

  current::FileSystem::WriteStringToFile(stream_golden_data, stream1_file_name.c_str());
  current::stream::MasterFlipController<stream_t> stream1(stream_t::CreateStream(stream1_file_name));
  // After construction the stream is always should be master.
  EXPECT_TRUE(stream1.IsMasterStream());
  auto flip_key1 = stream1.ExposeViaHTTP(port1, "/exposed");
  // Cannot expose the same stream twice. Why not, BTW?
  ASSERT_THROW(stream1.ExposeViaHTTP(port2, "/exposed_twice"), current::stream::StreamIsAlreadyExposedException);
  // Attempt to follow using invalid url should lead to an exception.
  ASSERT_THROW(stream1.FollowRemoteStream("fake_url"), current::net::SocketResolveAddressException);
  // Cannot flip stream if it's already in master mode.
  ASSERT_THROW(stream1.FlipToMaster(flip_key1), current::stream::StreamIsAlreadyMasterException);
  // Stream should remain a valid master after all these unsuccessful calls.
  EXPECT_TRUE(stream1.IsMasterStream());

  const std::string base_url = Printf("http://localhost:%d/exposed", port1);
  const std::string base_url2 = Printf("http://localhost:%d/exposed_follower", port2);

  current::stream::MasterFlipController<stream_t> stream2(stream_t::CreateStream(stream2_file_name));
  // The second stream is master now, so it has no reason to flip.
  ASSERT_THROW(stream2.FlipToMaster(flip_key1), current::stream::StreamIsAlreadyMasterException);
  auto publisher = stream2->BecomeFollowingStream();
  // Now the stream should be following.
  EXPECT_FALSE(stream2.IsMasterStream());
  // But it doesn't follow any remote stream, so it still can't perform the flip.
  ASSERT_THROW(stream2.FlipToMaster(flip_key1), current::stream::StreamDoesNotFollowAnyoneException);
  // Can't call the `FollowRemoteStream` now, cause it will hang in the `BecomeFollowingStream`.
  // stream2.FollowRemoteStream(base_url);
  publisher = nullptr;
  // And now, after the borrowed publisher was released, we can call `FollowRemoteStream`.
  ASSERT_THROW(stream2.FollowRemoteStream("invalid_url"), current::net::SocketResolveAddressException);
  // At last, this call should suceeded.
  stream2.FollowRemoteStream(base_url, current::stream::SubscriptionMode::Checked);
  // And the same stream can be exposed on a different endpoint.
  const auto flip_key2 = stream2.ExposeViaHTTP(port2, "/exposed_follower");
  // But it can't follow two remote streams simultaneously.
  ASSERT_THROW(stream2.FollowRemoteStream("fake_url"), current::stream::StreamIsAlreadyFollowingException);

  // First attempt to flip to master using the wrong key should fail.
  ASSERT_THROW(stream2.FlipToMaster(flip_key1 + 1), current::stream::RemoteStreamRefusedFlipRequestException);
  // Now the flip operation is blocked for 1 second, so the second try should also fail, although the key is correct.
  current::time::SetNow(current::time::Now() + std::chrono::milliseconds(500));
  ASSERT_THROW(stream2.FlipToMaster(flip_key1), current::stream::RemoteStreamRefusedFlipRequestException);
  current::time::SetNow(current::time::Now() + std::chrono::milliseconds(500));
  // The third attempt should succeed, as the key is correct and the previous attempt
  // with the wrong key was made more than 1s ago.
  stream2.FlipToMaster(flip_key1);
  // After the flip stream should be replicated completely.
  EXPECT_EQ(stream_golden_data_single_head, current::FileSystem::ReadFileAsString(stream2_file_name));
  // The second stream becomes master,
  // while the first one turns into a follower (but it doesn't follow anyone, actually).
  EXPECT_TRUE(stream2.IsMasterStream());
  EXPECT_FALSE(stream1.IsMasterStream());
  // One flip is enough, the second should fail, because the stream is master now.
  ASSERT_THROW(stream2.FlipToMaster(flip_key1), current::stream::StreamIsAlreadyMasterException);
  // The stream was exposed before and it keeps that endpoints alive after the flip.
  ASSERT_THROW(stream2.ExposeViaHTTP(port1, "/exposed"), current::stream::StreamIsAlreadyExposedException);
  // The first stream can't flip, because it doesn't automatically begin following the second stream
  // after the flip procedure.
  ASSERT_THROW(stream1.FlipToMaster(flip_key2), current::stream::StreamDoesNotFollowAnyoneException);
  stream1.StopExposingViaHTTP();
  stream1.FollowRemoteStream(base_url2);
  flip_key1 = stream1.ExposeViaHTTP(port1, "/exposed");

  current::stream::MasterFlipController<stream_t> stream3(stream_t::CreateStream(stream3_file_name));
  stream3.FollowRemoteStream(base_url);
  // Now we have stream3 following the stream1, which is following the stream2.
  // To flip stream3 with stream2 we should use key for the stream2, not the stream1.
  ASSERT_THROW(stream3.FlipToMaster(flip_key1), current::stream::RemoteStreamRefusedFlipRequestException);
  current::time::SetNow(current::time::Now() + std::chrono::seconds(1));
  stream3.FlipToMaster(flip_key2);
  EXPECT_EQ(stream_golden_data_single_head, current::FileSystem::ReadFileAsString(stream2_file_name));
  EXPECT_EQ(stream_golden_data_single_head, current::FileSystem::ReadFileAsString(stream3_file_name));
  EXPECT_FALSE(stream1.IsMasterStream());
  EXPECT_FALSE(stream2.IsMasterStream());
  EXPECT_TRUE(stream3.IsMasterStream());
}

TEST(Stream, SubscribeWithFilterByType) {
  current::time::ResetToZero();

  using namespace stream_unittest;

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

    EntryResponse operator()(const std::string& record_json, uint64_t, idxts_t) {
      const auto tab_pos = record_json.find('\t');
      CURRENT_ASSERT(tab_pos != std::string::npos);
      results_.push_back(record_json.c_str() + tab_pos + 1);
      return results_.size() == expected_count_ ? EntryResponse::Done : EntryResponse::More;
    }

    EntryResponse operator()(std::chrono::microseconds) const { return EntryResponse::More; }

    TerminationResponse Terminate() const { return TerminationResponse::Wait; }

    static EntryResponse EntryResponseIfNoMorePassTypeFilter() { return EntryResponse::More; }

    std::vector<std::string> results_;
    const size_t expected_count_;
  };

  auto stream = current::stream::Stream<Variant<Record, AnotherRecord>>::CreateStream();
  for (int i = 1; i <= 5; ++i) {
    current::time::SetNow(std::chrono::microseconds(i));
    if (i & 1) {
      stream->Publisher()->Publish(Record(i));
    } else {
      stream->Publisher()->Publish(AnotherRecord(i));
    }
  }

  {
    using Collector = current::ss::StreamSubscriber<CollectorImpl, Variant<Record, AnotherRecord>>;
    static_assert(current::ss::IsStreamSubscriber<Collector, Variant<Record, AnotherRecord>>::value, "");
    static_assert(!current::ss::IsStreamSubscriber<Collector, Record>::value, "");
    static_assert(!current::ss::IsStreamSubscriber<Collector, AnotherRecord>::value, "");

    Collector c(5);
    Collector c_unchecked(5);
    stream->Subscribe(c);
    stream->SubscribeUnchecked(c_unchecked);
    EXPECT_EQ(
        "{\"Record\":{\"x\":1}} {\"AnotherRecord\":{\"y\":2}} {\"Record\":{\"x\":3}} "
        "{\"AnotherRecord\":{\"y\":4}} {\"Record\":{\"x\":5}}",
        Join(c.results_, ' '));
    EXPECT_EQ(
        "{\"Record\":{\"x\":1},\"\":\"T9209980947553411947\"} "
        "{\"AnotherRecord\":{\"y\":2},\"\":\"T9201000647893547023\"} "
        "{\"Record\":{\"x\":3},\"\":\"T9209980947553411947\"} "
        "{\"AnotherRecord\":{\"y\":4},\"\":\"T9201000647893547023\"} "
        "{\"Record\":{\"x\":5},\"\":\"T9209980947553411947\"}",
        Join(c_unchecked.results_, ' '));
  }

  {
    using Collector = current::ss::StreamSubscriber<CollectorImpl, Record>;
    static_assert(!current::ss::IsStreamSubscriber<Collector, Variant<Record, AnotherRecord>>::value, "");
    static_assert(current::ss::IsStreamSubscriber<Collector, Record>::value, "");
    static_assert(!current::ss::IsStreamSubscriber<Collector, AnotherRecord>::value, "");

    Collector c(3);
    stream->Subscribe<Record>(c);
    EXPECT_EQ("X=1 X=3 X=5", Join(c.results_, ' '));
  }

  {
    using Collector = current::ss::StreamSubscriber<CollectorImpl, AnotherRecord>;
    static_assert(!current::ss::IsStreamSubscriber<Collector, Variant<Record, AnotherRecord>>::value, "");
    static_assert(!current::ss::IsStreamSubscriber<Collector, Record>::value, "");
    static_assert(current::ss::IsStreamSubscriber<Collector, AnotherRecord>::value, "");

    Collector c(2);
    stream->Subscribe<AnotherRecord>(c);
    EXPECT_EQ("Y=2 Y=4", Join(c.results_, ' '));
  }
}

TEST(Stream, ReleaseAndAcquirePublisher) {
  current::time::ResetToZero();

  using namespace stream_unittest;
  using Stream = current::stream::Stream<Record>;

  struct DynamicStreamPublisherAcquirer {
    using publisher_t = typename Stream::publisher_t;
    DynamicStreamPublisherAcquirer(current::Borrowed<publisher_t> publisher)
        : publisher_(std::move(publisher),
                     [this]() {
                       requested_termination_ = true;
                       publisher_ = nullptr;
                     }) {}
    bool requested_termination_ = false;
    current::BorrowedWithCallback<publisher_t> publisher_;
  };

  auto stream = Stream::CreateStream();
  Data d;
  {
    // In this test we start the subscriber before we publish anything into the stream.
    // That's why `idxts_t last` is not determined for each particular entry and we collect and check only the
    // values of `Record`s.
    StreamTestProcessor p(d, true);
    auto scope = stream->Subscribe(p);
    EXPECT_TRUE(static_cast<bool>(scope));
    {
      // Publish the first entry as usual.
      stream->Publisher()->Publish(Record(1), std::chrono::microseconds(100));

      {
        {
          // Transfer ownership of the stream publisher to the external object.
          EXPECT_TRUE(stream->IsMasterStream());
          current::BorrowedOfGuaranteedLifetime<typename Stream::publisher_t> acquired_publisher =
              stream->BecomeFollowingStream();
          EXPECT_FALSE(stream->IsMasterStream());

          // Publish to the stream is not allowed since the publisher has been moved away.
          ASSERT_THROW(stream->Publisher(), current::stream::PublisherNotAvailableException);
          ASSERT_THROW(stream->BorrowPublisher(), current::stream::PublisherNotAvailableException);

          // Now we can publish only via `acquired_publisher` that owns stream publisher object.
          acquired_publisher->Publish(Record(3), std::chrono::microseconds(300));

          // NOTE: `stream->BecomeFollowingStream()` will fail here, as `acquired_publisher` is still in scope.
          // Uncomment the next line to see the `BorrowedOfGuaranteedLifetimeInvariantErrorCallback.`.
          // stream->BecomeFollowingStream();
        }

        // Acquire the publisher back.
        // Note: An attempt to do so before `acquired_publisher` is out of scope will cause a deadlock.
        // See below for a more sophisticated test.
        EXPECT_FALSE(stream->IsMasterStream());
        stream->BecomeMasterStream();
        EXPECT_TRUE(stream->IsMasterStream());

        // Master becoming master is best to not throw. It's called from the destructor of the replicator, for instance.
        ASSERT_NO_THROW(stream->BecomeMasterStream());
      }

      {
        struct TemporaryPublisher {
          using publisher_t = typename Stream::publisher_t;
          current::BorrowedWithCallback<publisher_t> publisher_;
          bool termination_requested_ = false;
          TemporaryPublisher(current::Borrowed<publisher_t> publisher)
              : publisher_(publisher,
                           [this]() {
                             CURRENT_ASSERT(!termination_requested_);
                             termination_requested_ = true;
                             // Note: Comment out the next line and observe the deadlock in
                             // `stream->BecomeMasterStream();` below.
                             publisher_ = nullptr;
                           }) {}
        };

        // Transfer ownership of the stream publisher to the external object.
        EXPECT_TRUE(stream->IsMasterStream());
        TemporaryPublisher temporary_acquired_publisher(stream->BecomeFollowingStream());
        EXPECT_FALSE(stream->IsMasterStream());

        // Publish to the stream is not allowed since the publisher has been moved away.
        ASSERT_THROW(stream->Publisher(), current::stream::PublisherNotAvailableException);
        ASSERT_THROW(stream->BorrowPublisher(), current::stream::PublisherNotAvailableException);

        // Acquire the publisher back.
        // This time, as `TemporaryPublisher` supports graceful shutdown, there will be no deadlock.
        EXPECT_FALSE(stream->IsMasterStream());
        EXPECT_FALSE(temporary_acquired_publisher.termination_requested_);
        stream->BecomeMasterStream();
        EXPECT_TRUE(stream->IsMasterStream());
        EXPECT_TRUE(temporary_acquired_publisher.termination_requested_);
      }

      // Publish the third entry.
      stream->Publisher()->Publish(Record(4), std::chrono::microseconds(400));
    }

    while (d.seen_ < 3u) {
      std::this_thread::yield();
    }
    EXPECT_EQ(3u, d.seen_);
    EXPECT_EQ("1,3,4", d.results_);
    EXPECT_TRUE(d.subscriber_alive_);

    EXPECT_TRUE(static_cast<bool>(scope));

    {
      // Acquire the publisher again, but this time wait until it's requested to be returned.
      EXPECT_TRUE(stream->IsMasterStream());
      auto acquired_publisher = std::make_unique<DynamicStreamPublisherAcquirer>(stream->BecomeFollowingStream());
      EXPECT_FALSE(stream->IsMasterStream());

      // Publish to the stream is not allowed since the publisher has been moved.
      ASSERT_THROW(stream->Publisher(), current::stream::PublisherNotAvailableException);
      ASSERT_THROW(stream->BorrowPublisher(), current::stream::PublisherNotAvailableException);

      // Now we can publish only via `acquired_publisher` that owns stream publisher object.
      Value(acquired_publisher->publisher_)->Publish(Record(103), std::chrono::microseconds(10300));

      // Acquire the publisher back.
      EXPECT_FALSE(stream->IsMasterStream());
      ASSERT_FALSE(acquired_publisher->requested_termination_);
      stream->BecomeMasterStream();
      ASSERT_TRUE(acquired_publisher->requested_termination_);
      EXPECT_TRUE(stream->IsMasterStream());

      // Master becoming master is best to not throw. It's called from the destructor of the replicator, for instance.
      ASSERT_NO_THROW(stream->BecomeMasterStream());
    }

    while (d.seen_ < 4u) {
      std::this_thread::yield();
    }
    EXPECT_EQ(4u, d.seen_);
    EXPECT_EQ("1,3,4,103", d.results_);
    EXPECT_TRUE(d.subscriber_alive_);

    EXPECT_TRUE(static_cast<bool>(scope));
  }

  EXPECT_EQ("1,3,4,103,TERMINATE", d.results_);
  EXPECT_FALSE(d.subscriber_alive_);
}

TEST(Stream, SubscribingToJustTailDoesTheJob) {
  using namespace stream_unittest;

  auto exposed_stream = current::stream::Stream<Record>::CreateStream();
  const std::string base_url = Printf("http://localhost:%d/tail", FLAGS_stream_http_test_port);
  const auto scope = HTTP(FLAGS_stream_http_test_port).Register("/tail", *exposed_stream);

  std::thread initial_publisher([&]() {
    for (int i = 0; i <= 2; ++i) {
      exposed_stream->Publisher()->Publish(Record(i * 10), std::chrono::microseconds(i));
    }
  });

  while (HTTP(GET(base_url + "?sizeonly")).body != "3\n") {
    std::this_thread::yield();
  }

  initial_publisher.join();

  std::atomic_bool http_subscriber_started(false);

  std::thread follow_up_publisher([&]() {
    while (!http_subscriber_started) {
      ;  // Spin lock, begin publishing only once the HTTP subscriber has started.
    }
    for (int i = 3; i <= 5; ++i) {
      exposed_stream->Publisher()->Publish(Record(i * 10), std::chrono::microseconds(i));
    }
  });

  std::string body;
  HTTP(ChunkedGET(base_url + "?tail&n=3&array",
                  [&](const std::string& header, const std::string& value) {
                    static_cast<void>(header);
                    static_cast<void>(value);
                    http_subscriber_started = true;
                  },
                  [&](const std::string& chunk_body) { body += chunk_body; }));
  follow_up_publisher.join();

  const auto result = ParseJSON<std::vector<Record>>(body);

  ASSERT_EQ(3u, result.size());
  EXPECT_EQ(30, result[0].x);
  EXPECT_EQ(40, result[1].x);
  EXPECT_EQ(50, result[2].x);
}
