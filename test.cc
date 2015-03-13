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

#include "sherlock.h"

#include <string>
#include <atomic>
#include <thread>

#include "../Bricks/strings/util.h"
#include "../Bricks/cerealize/cerealize.h"
#include "../Bricks/net/api/api.h"

#include "../Bricks/dflags/dflags.h"
#include "../Bricks/3party/gtest/gtest-main-with-dflags.h"

DEFINE_int32(sherlock_http_test_port, 8090, "Local port to use for Sherlock unit test.");

using std::string;
using std::atomic_bool;
using std::atomic_size_t;
using std::thread;
using std::this_thread::sleep_for;
using std::chrono::milliseconds;

using bricks::strings::Printf;
using bricks::strings::ToString;

// The records we work with.
// TODO(dkorolev): Support and test polymorphic types.
struct Record {
  // TODO(dkorolev): Support and test timestamps.
  int x_;
  Record(int x = 0) : x_(x) {}
  template <typename A>
  void serialize(A& ar) {
    ar(cereal::make_nvp("x", x_));
  }
};

// Struct `Data` should be outside struct `Processor`, since the latter is `std::move`-d away in some tests.
struct Data {
  atomic_size_t seen_;
  string results_;
  Data() : seen_(0u) {}
};

// Struct `Processor` handles the entries that tests subscribe to.
struct Processor {
  Data& data_;
  size_t max_to_process_ = static_cast<size_t>(-1);

  Processor() = delete;
  explicit Processor(Data& data) : data_(data) {}

  Processor& SetMax(size_t cap) {
    max_to_process_ = cap;
    return *this;
  }

  inline bool Entry(const Record& entry) {
    if (data_.seen_) {
      data_.results_ += ",";
    }
    data_.results_ += ToString(entry.x_);
    ++data_.seen_;
    return data_.seen_ < max_to_process_;
  }

  inline void Terminate() {
    if (data_.seen_) {
      data_.results_ += ",";
    }
    data_.results_ += "DONE";
  }
};

TEST(Sherlock, SubscribeAndProcessThreeEntries) {
  auto foo_stream = sherlock::Stream<Record>("foo");
  foo_stream.Publish(1);
  foo_stream.Publish(2);
  foo_stream.Publish(3);
  Data d;
  Processor p(d);
  foo_stream.Subscribe(p.SetMax(3u))
      .Join();  // `.Join()` makes this a blocking call, waiting for three entries.
  EXPECT_EQ(d.seen_, 3u);
  EXPECT_EQ("1,2,3", d.results_);
}

TEST(Sherlock, SubscribeAndProcessThreeEntriesByUniquePtr) {
  auto bar_stream = sherlock::Stream<Record>("bar");
  bar_stream.Publish(4);
  bar_stream.Publish(5);
  bar_stream.Publish(6);
  Data d;
  std::unique_ptr<Processor> p(new Processor(d));
  p->SetMax(3u);
  bar_stream.Subscribe(std::move(p))
      .Join();  // `.Join()` makes this a blocking call, waiting for three entries.
  EXPECT_EQ(d.seen_, 3u);
  EXPECT_EQ("4,5,6", d.results_);
}

TEST(Sherlock, SubscribeHandleGoesOutOfScopeBeforeAnyProcessing) {
  auto baz_stream = sherlock::Stream<Record>("baz");
  thread delayed_publish_thread([&baz_stream]() {
    sleep_for(milliseconds(10));
    baz_stream.Publish(7);
    baz_stream.Publish(8);
    baz_stream.Publish(9);
  });
  Data d;
  Processor p(d);
  baz_stream.Subscribe(p);
  EXPECT_EQ(d.seen_, 0u);
  EXPECT_EQ("DONE", d.results_);
  delayed_publish_thread.join();
}

TEST(Sherlock, SubscribeProcessedThreeEntriesBecauseWeWaitInTheScope) {
  auto meh_stream = sherlock::Stream<Record>("meh");
  meh_stream.Publish(10);
  meh_stream.Publish(11);
  meh_stream.Publish(12);
  Data d;
  Processor p(d);
  {
    auto scope = meh_stream.Subscribe(p);
    {
      auto scope2 = std::move(scope);
      {
        auto scope3 = std::move(scope2);
        while (d.seen_ < 3u) {
          ;  // Spin lock.
        }
      }
    }
  }
  EXPECT_EQ(d.seen_, 3u);
  EXPECT_EQ("10,11,12,DONE", d.results_);
}
