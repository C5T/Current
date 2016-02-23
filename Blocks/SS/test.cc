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

#define CURRENT_MOCK_TIME  // `SetNow()`.

#include "ss.h"

#include <deque>
#include <string>

#include "../../Bricks/time/chrono.h"
#include "../../Bricks/strings/join.h"
#include "../../3rdparty/gtest/gtest-main.h"

namespace stream_system_test {

// A copy-able and move-able entry.
struct DispatchDemoEntry {
  std::string text;
  DispatchDemoEntry() : text("Entry()") {}
  DispatchDemoEntry(const std::string& text) : text("Entry('" + text + "')") {}
  DispatchDemoEntry(const DispatchDemoEntry& rhs) : text("Entry(copy of {" + rhs.text + "})") {}
  DispatchDemoEntry(DispatchDemoEntry&& rhs) : text("Entry(move of {" + rhs.text + "})") {
    rhs.text = "moved away {" + rhs.text + "}";
  }

  DispatchDemoEntry& operator=(const DispatchDemoEntry&) = delete;
  DispatchDemoEntry& operator=(DispatchDemoEntry&&) = delete;
};

}  // namespace stream_system_test

TEST(StreamSystem, TestHelperClassSmokeTest) {
  using DDE = stream_system_test::DispatchDemoEntry;

  DDE e1;
  EXPECT_EQ("Entry()", e1.text);

  DDE e2("E2");
  EXPECT_EQ("Entry('E2')", e2.text);

  DDE e3(e2);
  EXPECT_EQ("Entry(copy of {Entry('E2')})", e3.text);
  EXPECT_EQ("Entry('E2')", e2.text);

  DDE e4(std::move(e2));
  EXPECT_EQ("Entry(move of {Entry('E2')})", e4.text);
  EXPECT_EQ("moved away {Entry('E2')}", e2.text);
}

namespace stream_system_test {

struct DemoEntryPublisherImpl {
  using IDX_TS = current::ss::IndexAndTimestamp;
  size_t index = 0u;
  std::deque<DispatchDemoEntry> values;  // Use `deque` to avoid extra copies.

  IDX_TS DoPublish(const DispatchDemoEntry& e, std::chrono::microseconds) {
    ++index;
    values.emplace_back(e);
    return IDX_TS(index, current::time::Now());
  }

  IDX_TS DoPublish(DispatchDemoEntry&& e, std::chrono::microseconds) {
    ++index;
    values.emplace_back(std::move(e));
    return IDX_TS(index, current::time::Now());
  }

  // TODO: Decide on `Emplace`.
  //  IDX_TS DoEmplace(const std::string& text) {
  //    ++index;
  //    values.emplace_back(text);
  //    return IDX_TS(index, current::time::Now());
  //  }
};

}  // namespace stream_system_test

TEST(StreamSystem, EntryPublisher) {
  using namespace stream_system_test;
  using DDE = DispatchDemoEntry;
  using EntryPublisher = current::ss::EntryPublisher<DemoEntryPublisherImpl, DDE>;

  EntryPublisher publisher;
  {
    DDE e1("E1");
    const DDE& e1_cref = e1;
    current::time::SetNow(std::chrono::microseconds(100));
    const auto result = publisher.Publish(e1_cref);
    EXPECT_EQ(1u, result.index);
    EXPECT_EQ(100, result.us.count());
  }
  {
    DDE e2("E2");
    current::time::SetNow(std::chrono::microseconds(200));
    const auto result = publisher.Publish(std::move(e2));
    EXPECT_EQ(2u, result.index);
    EXPECT_EQ(200, result.us.count());
  }
  // TODO: Decide on `Emplace`.
  //  {
  //    current::time::SetNow(std::chrono::microseconds(300));
  //    const auto result = publisher.Emplace("E3");
  //    EXPECT_EQ(3u, result.index);
  //    EXPECT_EQ(300, result.us.count());
  //  }

  std::string all_values;
  bool first = true;
  for (const auto& e : publisher.values) {
    if (first) {
      first = false;
    } else {
      all_values += ',';
    }
    all_values += e.text;
  }
  EXPECT_EQ("Entry(copy of {Entry('E1')}),Entry(move of {Entry('E2')})", all_values);
}
