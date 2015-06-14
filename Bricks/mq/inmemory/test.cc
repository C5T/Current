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

#include "mq.h"

#include "../../../3rdparty/gtest/gtest-main.h"
#include "../../strings/printf.h"

#include <atomic>
#include <thread>
#include <chrono>

using bricks::mq::MMQ;

TEST(InMemoryMQ, SmokeTest) {
  struct Consumer {
    std::string messages_;
    size_t expected_next_message_index_ = 0u;
    size_t dropped_messages_ = 0u;
    std::atomic_size_t processed_messages_;
    Consumer() : processed_messages_(0u) {}
    void operator()(const std::string& s, size_t index) {
      assert(index >= expected_next_message_index_);
      dropped_messages_ += (index - expected_next_message_index_);
      expected_next_message_index_ = (index + 1);
      messages_ += s + '\n';
      ++processed_messages_;
      assert(expected_next_message_index_ - processed_messages_ == dropped_messages_);
    }
  };

  Consumer c;
  MMQ<std::string, Consumer> mmq(c);
  mmq.PushMessage("one");
  mmq.PushMessage("two");
  mmq.PushMessage("three");
  while (c.processed_messages_ != 3) {
    ;  // Spin lock;
  }
  EXPECT_EQ("one\ntwo\nthree\n", c.messages_);
  EXPECT_EQ(0u, c.dropped_messages_);
}

struct SlowConsumer {
  std::vector<std::string> messages_;
  std::atomic_size_t processed_messages_;
  size_t total_messages_ = 0u;
  bool at_least_one_message_dropped_ = false;
  size_t delay_ms_;
  SlowConsumer(size_t delay_ms = 20u) : processed_messages_(0u), delay_ms_(delay_ms) {}
  void operator()(const std::string& s, size_t index, size_t total) {
    at_least_one_message_dropped_ |= (index != processed_messages_);
    messages_.push_back(s);
    // Simulate slow message processing by adding a delay.
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms_));
    assert(total >= total_messages_);
    total_messages_ = total;
    ++processed_messages_;
  }
};

TEST(InMemoryMQ, DropOnOverflowTest) {
  SlowConsumer c;

  // Queue with 10 events in the buffer.
  MMQ<std::string, SlowConsumer, 10, true> mmq(c);

  // Push 20 events one after another, causing an overflow, which would drop some messages.
  for (size_t i = 0; i < 20; ++i) {
    mmq.PushMessage(bricks::strings::Printf("M%02d", static_cast<int>(i)));
  }

  // Confirm that exactly 10 messages were processed and exactly 10 were dropped due to a very slow consumer.
  // Despite being flaky, this condition can only fail if it takes more than 20 ms to add the messages,
  // which is quite huge time interval.
  c.delay_ms_ = 0u;
  while (c.processed_messages_ != 10u) {
    ;  // Spin lock.
  }

  EXPECT_FALSE(c.at_least_one_message_dropped_);

  // Ensure that all processed messages are indeed unique.
  std::set<std::string> messages(begin(c.messages_), end(c.messages_));
  EXPECT_EQ(messages.size(), 10u);
  EXPECT_EQ(c.total_messages_, 20u);  // Only 10 of 20 messages have been processed, the rest were dropped.

  // To confirm messages are being dropped, add another one.
  // This will call `operator()` with an up-to-date counter, indicating lost messages.
  mmq.PushMessage("Plus one");
  while (c.processed_messages_ != 11u) {
    ;  // Spin lock.
  }
  EXPECT_TRUE(c.at_least_one_message_dropped_);
  EXPECT_EQ(c.messages_.size(), 11u);
  EXPECT_EQ(c.total_messages_, 21u);  // Only 10 of 20 messages have been processed, the rest were dropped.
}

TEST(InMemoryMQ, WaitOnOverflowTest) {
  SlowConsumer c(1u);

  // Queue with 10 events in the buffer.
  MMQ<std::string, SlowConsumer, 10> mmq(c);

  const auto producer = [&](char prefix, size_t count) {
    for (size_t i = 0; i < count; ++i) {
      mmq.PushMessage(bricks::strings::Printf("%c%02d", prefix, static_cast<int>(i)));
    }
  };

  std::vector<std::thread> producers;

  for (size_t i = 0; i < 10; ++i) {
    producers.emplace_back(producer, 'a' + i, 10);
  }

  for (auto& p : producers) {
    p.join();
  }

  // Since we push 100 messages and the size of the buffer is 10,
  // we must see at least 90 messages processed by this moment.
  EXPECT_GE(c.processed_messages_, 90u);

  // Wait until the rest of the queued messages are processed.
  while (c.processed_messages_ != 100u) {
    ;  // Spin lock;
  }

  // Confirm that none of the messages were dropped.
  EXPECT_EQ(c.processed_messages_, c.total_messages_);

  // Ensure that all processed messages are indeed unique.
  std::set<std::string> messages(begin(c.messages_), end(c.messages_));
  EXPECT_EQ(messages.size(), 100u);
}
