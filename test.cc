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

// TODO(dkorolev): Support and test timestamps.
struct SortOfInt {
  int x_;
  SortOfInt(int x = 0) : x_(x) {}
};

// The `Nice` processor terminates right away when the listener goes out of scope.
struct NiceProcessorOfSortOfInts {
  atomic_size_t seen_;
  string results_;
  NiceProcessorOfSortOfInts() : seen_(0) {}
  inline bool Entry(const SortOfInt& entry) {
    if (seen_) {
      results_ += ",";
    }
    results_ += ToString(entry.x_);
    ++seen_;
    sleep_for(milliseconds(1));
    return true;
  }
  inline bool TerminationRequest() { return false; }
};

// The `Waiting` processor ignores the termination request and waits until three entries have been seen.
struct WaitingProcessorOfSortOfInts {
  atomic_size_t seen_;
  string results_;
  WaitingProcessorOfSortOfInts() : seen_(0) {}
  inline bool Entry(const SortOfInt& entry) {
    if (seen_) {
      results_ += ",";
    }
    results_ += ToString(entry.x_);
    ++seen_;
    sleep_for(milliseconds(1));
    return (seen_ < 3);
  }
  inline bool TerminationRequest() { return true; }
};

TEST(Sherlock, NiceProcessorOfInts) {
  auto foo_stream = sherlock::Stream<SortOfInt>("foo");
  foo_stream.Publish(1);
  foo_stream.Publish(2);
  foo_stream.Publish(3);
  NiceProcessorOfSortOfInts p;
  auto scope = foo_stream.Subscribe(p);
  static_cast<void>(scope);
  // The `Nice*` processor will terminate right away, so it'll see one entry at best.
  EXPECT_LE(p.seen_, 1u);
  // Don't run more checks here since the processor is still running,
  // and thus the results may be flaky.
}

TEST(Sherlock, WaitingProcessorOfInts) {
  auto bar_stream = sherlock::Stream<SortOfInt>("bar");
  bar_stream.Publish(4);
  bar_stream.Publish(5);
  bar_stream.Publish(6);
  WaitingProcessorOfSortOfInts p;
  // Calling `Subscribe()` without capturing the results attempts to terminate the listener right away.
  bar_stream.Subscribe(p);  // This call blocks until the processor declared itself done.
  // Unlike the `Nice*` processor, the `Waiting*` one will wait for three entries to get processed.
  EXPECT_EQ(p.seen_, 3u);
  EXPECT_EQ("4,5,6", p.results_);
}

TEST(Sherlock, WaitingProcessorOfIntsReallyWaits) {
  auto baz_stream = sherlock::Stream<SortOfInt>("baz");
  thread delayed_publishing_thread([&baz_stream]() {
    sleep_for(milliseconds(5));
    baz_stream.Publish(7);
    baz_stream.Publish(8);
    baz_stream.Publish(9);
  });
  WaitingProcessorOfSortOfInts p;
  baz_stream.Subscribe(p);  // This call blocks until the processor declared itself done.
  // Unlike the `Nice*` processor, the `Waiting*` one will wait for three entries to get processed.
  EXPECT_EQ(p.seen_, 3u);
  EXPECT_EQ("7,8,9", p.results_);
  delayed_publishing_thread.join();
}

// TODO(dkorolev): Support and test `CaughtUp()`: Add a few pre-populated plus a few delayed entries.

struct Point {
  int x;
  int y;
  template <typename A>
  void serialize(A& ar) {
    ar(CEREAL_NVP(x), CEREAL_NVP(y));
  }
};

// TODO(dkorolev): This class should be moved into `sherlock.h`.
template <typename T>
class ServeJSONOverHTTP {
 public:
  ServeJSONOverHTTP(Request&& r)
      : http_request_scope_(std::move(r)), http_response_(http_request_scope_.SendChunkedResponse()) {}

  inline bool Entry(const T& entry) {
    http_response_(JSON(entry, "point"));  // TODO(dkorolev): WTF do I have to say JSON() here?
    ++n_;
    keep_going_ = (n_ < 3);
    return keep_going_;
  }

  inline bool TerminationRequest() {
    return true;  // Run forever (TODO(dkorolev): No, actually, return false here!
  }

  // TODO(dkorolev): This is the hack to make the test pass.
  atomic_bool keep_going_;

 private:
  Request http_request_scope_;  // Need to keep `Request` in scope, for the lifetime of the chunked response.
  bricks::net::HTTPServerConnection::ChunkedResponseSender http_response_;
  size_t n_ = 0;  // This is a hack to actually return after 10 entries.

  ServeJSONOverHTTP() = delete;
  ServeJSONOverHTTP(const ServeJSONOverHTTP&) = delete;
  void operator=(const ServeJSONOverHTTP&) = delete;
  ServeJSONOverHTTP(ServeJSONOverHTTP&&) = delete;
  void operator=(ServeJSONOverHTTP&&) = delete;
};

TEST(Sherlock, JSONOverHTTP) {
  auto time_series = sherlock::Stream<Point>("time_series");
  thread delayed_publishing_thread([&time_series]() {
    for (int x = 10; x <= 13; ++x) {
      sleep_for(milliseconds(5));
      time_series.Publish(Point{x, x * x});
    }
  });
  HTTP(FLAGS_sherlock_http_test_port).Register("/time_series", [&time_series](Request r) {
    // TODO(dkorolev): Figure out how to best handle ownership of the parameter wrt destruction.
    // For now it's a hack.
    ServeJSONOverHTTP<Point> server(std::move(r));
    auto tmp = time_series.Subscribe(server);  // TODO(dkorolev): `->Detach()`.
    while (server.keep_going_) {
      ;
    }
  });
  EXPECT_EQ("{\"point\":{\"x\":10,\"y\":100}}{\"point\":{\"x\":11,\"y\":121}}{\"point\":{\"x\":12,\"y\":144}}",
            HTTP(GET(Printf("http://localhost:%d/time_series", FLAGS_sherlock_http_test_port))).body);
  delayed_publishing_thread.join();
  HTTP(FLAGS_sherlock_http_test_port)
      .UnRegister("/time_series");  // TODO(dkorolev): Make use of a scoped version.
}
