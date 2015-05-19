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

#include <string>
#include <atomic>
#include <thread>
#include <tuple>

#include "../../yoda.h"
#include "../../test_types.h"

#include "../../../../Bricks/dflags/dflags.h"
#include "../../../../Bricks/3party/gtest/gtest-main-with-dflags.h"

DEFINE_int32(yoda_key_entry_test_port, 8992, "");

using std::string;
using std::atomic_size_t;
using bricks::strings::Printf;
using yoda::sfinae::is_same_or_compile_error;

struct KeyValueSubscriptionData {
  atomic_size_t seen_;
  string results_;
  KeyValueSubscriptionData() : seen_(0u) {}
};

struct KeyValueAggregateListener {
  KeyValueSubscriptionData& data_;
  size_t max_to_process_ = static_cast<size_t>(-1);

  KeyValueAggregateListener() = delete;
  explicit KeyValueAggregateListener(KeyValueSubscriptionData& data) : data_(data) {}

  KeyValueAggregateListener& SetMax(size_t cap) {
    max_to_process_ = cap;
    return *this;
  }

  bool Entry(std::unique_ptr<Padawan>& proto_entry, size_t index, size_t total) {
    // This is a _safe_ way to process entries, since if the cast fails, the next line will crash.
    const KeyValueEntry& entry = *dynamic_cast<KeyValueEntry*>(proto_entry.get());
    static_cast<void>(index);
    static_cast<void>(total);
    if (data_.seen_) {
      data_.results_ += ",";
    }
    data_.results_ += Printf("%d=%.2lf", entry.key, entry.value);
    ++data_.seen_;
    return data_.seen_ < max_to_process_;
  }
};

TEST(YodaKeyEntry, Smoke) {
  typedef yoda::API<yoda::KeyEntry<KeyValueEntry>> TestAPI;
  TestAPI api("YodaKeyEntrySmokeTest");

  api.AsyncAdd(KeyValueEntry(2, 0.5)).Wait();

  // Future expanded syntax.
  yoda::Future<KeyValueEntry> f1 = api.AsyncGet(2);
  KeyValueEntry r1 = f1.Go();
  EXPECT_EQ(2, r1.key);
  EXPECT_EQ(0.5, r1.value);

  // Future short syntax.
  EXPECT_EQ(0.5, api.AsyncGet(2).Go().value);

  // Callback version.
  struct CallbackTest {
    explicit CallbackTest(int key, double value, bool expect_success = true)
        : called(false), key(key), value(value), expect_success(expect_success) {}

    void found(const KeyValueEntry& entry) const {
      ASSERT_FALSE(called);
      called = true;
      EXPECT_TRUE(expect_success);
      EXPECT_EQ(key, entry.key);
      EXPECT_EQ(value, entry.value);
    }
    void not_found(const int key) const {
      ASSERT_FALSE(called);
      called = true;
      EXPECT_FALSE(expect_success);
      EXPECT_EQ(this->key, key);
    }
    void added() const {
      ASSERT_FALSE(called);
      called = true;
      EXPECT_TRUE(expect_success);
    }
    void already_exists() const {
      ASSERT_FALSE(called);
      called = true;
      EXPECT_FALSE(expect_success);
    }

    mutable std::atomic_bool called;
    const int key;
    const double value;
    const bool expect_success;
  };

  const CallbackTest cbt1(2, 0.5);
  api.AsyncGet(2,
               std::bind(&CallbackTest::found, &cbt1, std::placeholders::_1),
               std::bind(&CallbackTest::not_found, &cbt1, std::placeholders::_1));
  while (!cbt1.called) {
    ;  // Spin lock.
  }

  // Add two more key-value pairs.
  api.UnsafeStream().Emplace(new KeyValueEntry(3, 0.33));
  api.UnsafeStream().Emplace(new KeyValueEntry(4, 0.25));

  while (api.EntriesSeen() < 3u) {
    // For the purposes of this test: Spin lock to ensure that the listener/MMQ consumer got the data published.
  }

  EXPECT_EQ(0.33, api.AsyncGet(3).Go().value);
  EXPECT_EQ(0.25, api.Get(4).value);

  ASSERT_THROW(api.AsyncGet(5).Go(), yoda::KeyEntry<KeyValueEntry>::T_KEY_NOT_FOUND_EXCEPTION);
  ASSERT_THROW(api.AsyncGet(5).Go(), yoda::KeyNotFoundCoverException);
  ASSERT_THROW(api.Get(6), yoda::KeyEntry<KeyValueEntry>::T_KEY_NOT_FOUND_EXCEPTION);
  ASSERT_THROW(api.Get(6), yoda::KeyNotFoundCoverException);
  const CallbackTest cbt2(7, 0.0, false);
  api.AsyncGet(7,
               std::bind(&CallbackTest::found, &cbt2, std::placeholders::_1),
               std::bind(&CallbackTest::not_found, &cbt2, std::placeholders::_1));
  while (!cbt2.called) {
    ;  // Spin lock.
  }

  // Add three more key-value pairs, this time via the API.
  api.AsyncAdd(KeyValueEntry(5, 0.2)).Wait();
  api.Add(KeyValueEntry(6, 0.17));
  const CallbackTest cbt3(7, 0.76);
  api.AsyncAdd(yoda::KeyEntry<KeyValueEntry>::T_ENTRY(7, 0.76),
               std::bind(&CallbackTest::added, &cbt3),
               std::bind(&CallbackTest::already_exists, &cbt3));
  while (!cbt3.called) {
    ;  // Spin lock.
  }

  // Check that default policy doesn't allow overwriting on Add().
  ASSERT_THROW(api.AsyncAdd(KeyValueEntry(5, 1.1)).Go(),
               yoda::KeyEntry<KeyValueEntry>::T_KEY_ALREADY_EXISTS_EXCEPTION);
  ASSERT_THROW(api.AsyncAdd(KeyValueEntry(5, 1.1)).Go(), yoda::KeyAlreadyExistsCoverException);
  ASSERT_THROW(api.Add(KeyValueEntry(6, 0.28)), yoda::KeyEntry<KeyValueEntry>::T_KEY_ALREADY_EXISTS_EXCEPTION);
  ASSERT_THROW(api.Add(KeyValueEntry(6, 0.28)), yoda::KeyAlreadyExistsCoverException);
  const CallbackTest cbt4(7, 0.0, false);
  api.AsyncAdd(KeyValueEntry(7, 0.0),
               std::bind(&CallbackTest::added, &cbt4),
               std::bind(&CallbackTest::already_exists, &cbt4));
  while (!cbt4.called) {
    ;  // Spin lock.
  }

  // Thanks to eventual consistency, we don't have to wait until the above calls fully propagate.
  // Even if the next two lines run before the entries are published into the stream,
  // the API will maintain the consistency of its own responses from its own in-memory state.
  EXPECT_EQ(0.20, api.AsyncGet(5).Go().value);
  EXPECT_EQ(0.17, api.Get(6).value);

  ASSERT_THROW(api.AsyncGet(8).Go(), yoda::KeyEntry<KeyValueEntry>::T_KEY_NOT_FOUND_EXCEPTION);
  ASSERT_THROW(api.Get(9), yoda::KeyEntry<KeyValueEntry>::T_KEY_NOT_FOUND_EXCEPTION);

  // Test `Call`-based access.
  api.Call([](TestAPI::T_DATA data) {
             auto entries = yoda::KeyEntry<KeyValueEntry>::Accessor(data);

             EXPECT_FALSE(entries.Exists(1));
             EXPECT_TRUE(entries.Exists(2));

             // `Get()` syntax.
             EXPECT_EQ(0.33, static_cast<const KeyValueEntry&>(entries.Get(3)).value);
             EXPECT_FALSE(entries.Get(103));

             // `operator[]` syntax.
             EXPECT_EQ(0.25, static_cast<const KeyValueEntry&>(entries[4]).value);
             ASSERT_THROW(static_cast<void>(static_cast<const KeyValueEntry&>(entries[104])),
                          yoda::KeyNotFoundCoverException);

             auto mutable_entries = yoda::KeyEntry<KeyValueEntry>::Mutator(data);
             mutable_entries.Add(KeyValueEntry(50, 0.02));
             EXPECT_EQ(0.02, static_cast<const KeyValueEntry&>(entries[50]).value);
             EXPECT_EQ(0.02, static_cast<const KeyValueEntry&>(mutable_entries[50]).value);
             EXPECT_EQ(0.02, static_cast<const KeyValueEntry&>(entries.Get(50)).value);
             EXPECT_EQ(0.02, static_cast<const KeyValueEntry&>(mutable_entries.Get(50)).value);
           }).Wait();

  // Confirm that data updates have been pubished as stream entries as well.
  // This part is important since otherwise the API is no better than a wrapper over a hash map.
  KeyValueSubscriptionData data;
  KeyValueAggregateListener listener(data);
  listener.SetMax(7u);
  {
    auto scope = api.UnsafeStream().SyncSubscribe(listener);
    while (data.seen_ < 7u) {
      ;  // Spin lock.
    }
    scope.Join();
  }
  EXPECT_EQ(data.seen_, 7u);
  EXPECT_EQ("2=0.50,3=0.33,4=0.25,5=0.20,6=0.17,7=0.76,50=0.02", data.results_);

  // Confirm that the stream can be HTTP-listened to.
  HTTP(FLAGS_yoda_key_entry_test_port).ResetAllHandlers();
  api.ExposeViaHTTP(FLAGS_yoda_key_entry_test_port, "/data");
  const std::string Z = "";  // For `clang-format`-indentation purposes.
  EXPECT_EQ(Z + JSON(WithBaseType<Padawan>(KeyValueEntry(2, 0.50)), "entry") + '\n' +
                JSON(WithBaseType<Padawan>(KeyValueEntry(3, 0.33)), "entry") + '\n' +
                JSON(WithBaseType<Padawan>(KeyValueEntry(4, 0.25)), "entry") + '\n' +
                JSON(WithBaseType<Padawan>(KeyValueEntry(5, 0.20)), "entry") + '\n' +
                JSON(WithBaseType<Padawan>(KeyValueEntry(6, 0.17)), "entry") + '\n' +
                JSON(WithBaseType<Padawan>(KeyValueEntry(7, 0.76)), "entry") + '\n' +
                JSON(WithBaseType<Padawan>(KeyValueEntry(50, 0.02)), "entry") + '\n',
            HTTP(GET(Printf("http://localhost:%d/data?cap=7", FLAGS_yoda_key_entry_test_port))).body);
}
