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

#include "mq.h"

#include "../../3party/gtest/gtest-main.h"
#include "../../strings/printf.h"

#include <atomic>
#include <thread>
#include <chrono>

TEST(InMemoryMQ, SmokeTest) {
  struct Consumer {
    std::string messages_;
    size_t dropped_messages_ = 0u;
    std::atomic_size_t processed_messages_;
    Consumer() : processed_messages_(0u) {}
    void OnMessage(const std::string& s, size_t dropped_messages) {
      messages_ += s + '\n';
      dropped_messages_ += dropped_messages;
      ++processed_messages_;
    }
  };

  Consumer c;
  MMQ<Consumer> mmq(c);
  mmq.PushMessage("one");
  mmq.PushMessage("two");
  mmq.PushMessage("three");
  while (c.processed_messages_ != 3) {
    ;  // Spin lock;
  }
  EXPECT_EQ("one\ntwo\nthree\n", c.messages_);
  EXPECT_EQ(0u, c.dropped_messages_);
}

TEST(InMemoryMQ, DropsMessagesTest) {
  struct SlowConsumer {
    std::string messages_;
    size_t dropped_messages_ = 0u;
    std::atomic_size_t processed_messages_;
    std::atomic_size_t processing_time_ms_;
    SlowConsumer() : processed_messages_(0u), processing_time_ms_(0u) {}
    void OnMessage(const std::string& s, size_t dropped_messages) {
      messages_ += s + '\n';
      dropped_messages_ += dropped_messages;
      // Simulate message processing time of 1ms.
      std::this_thread::sleep_for(std::chrono::milliseconds(processing_time_ms_));
      ++processed_messages_;
    }
  };

  SlowConsumer c;
  // Queue with 10 events in the buffer.
  MMQ<SlowConsumer, std::string, 10> mmq(c);
  // Push 50 events one after another, causing an overflow, which would drop some messages.
  for (size_t i = 0; i < 50; ++i) {
    mmq.PushMessage(bricks::strings::Printf("M%03d", static_cast<int>(i)));
  }
  // Wait until at least two messages are processed by the consumer.
  // The second one will get its `dropped_messages_` count set.
  // (The first one might too, but that's uncertain.
  while (c.processed_messages_ != 2) {
    ;  // Spin lock;
  }
  // Confirm that some messages were indeed dropped.
  EXPECT_GT(c.dropped_messages_, 0u);
  EXPECT_LT(c.dropped_messages_, 50u);

  // Without the next line the test will run for 10+ms, since the remaining queue
  // is being processed in full in the destructor of MMQ.
  c.processing_time_ms_ = 0u;
}
