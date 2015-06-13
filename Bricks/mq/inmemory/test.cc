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
  std::atomic_size_t dropped_messages_;
  std::atomic_size_t processed_messages_;
  size_t delay_ms_;
  SlowConsumer(size_t delay_ms = 20u) : dropped_messages_(0u), processed_messages_(0u), delay_ms_(delay_ms) {}
  // Uncomment the line below to see that it doesn't compile.
  // void OnMessage(const std::string& s) {}
  void OnMessage(const std::string& s, size_t dropped_messages) {
    messages_.push_back(s);
    dropped_messages_ += dropped_messages;
    // Simulate slow message processing by adding a delay.
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms_));
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
  // Wait until at least three messages are processed by the consumer to
  // properly get dropped messages count.
  while (c.processed_messages_ < 2) {
    ;  // Spin lock;
  }

  // Confirm that exactly 10 messages were dropped due to a very slow consumer.
  // Despite being flaky, this condition can only fail if it takes more than 20 ms to add the messages,
  // which is quite huge time interval.
  EXPECT_EQ(c.dropped_messages_, 10u);

  // Wait until the rest of the queued messages are processed.
  c.delay_ms_ = 0u;
  while (c.processed_messages_ != 10u) {
    ;  // Spin lock;
  }

  // Ensure that all processed messages are indeed unique.
  std::set<std::string> messages(begin(c.messages_), end(c.messages_));
  EXPECT_EQ(messages.size(), 10u);
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

  // Since we push 100 messages and the size of the buffer is 10, we must see at least 90 messages processed by
  // this moment.
  EXPECT_GE(c.processed_messages_, 90u);

  // Wait until the rest of the queued messages are processed.
  while (c.processed_messages_ != 100u) {
    ;  // Spin lock;
  }

  // Confirm that none of the messages were dropped.
  EXPECT_EQ(c.dropped_messages_, 0u);

  // Ensure that all processed messages are indeed unique.
  std::set<std::string> messages(begin(c.messages_), end(c.messages_));
  EXPECT_EQ(messages.size(), 100u);
}

TEST(InMemoryMQ, DefaultConsumer) {
  struct Storage {
    std::string messages;
    std::atomic_size_t count;
    Storage() : count(0u) {}
  };

  struct ProcessableMessage {
    Storage* storage;
    std::string message;

    ProcessableMessage() = default;
    ProcessableMessage(Storage* storage, const std::string& message) : storage(storage), message(message) {}

    void Process() {
      storage->messages += message;
      ++storage->count;
    }
  };

  Storage storage;

  MMQ<std::unique_ptr<ProcessableMessage>> mmq1;
  mmq1.EmplaceMessage(new ProcessableMessage(&storage, "This "));
  mmq1.EmplaceMessage(new ProcessableMessage(&storage, "is"));
  while (storage.count != 2) {
    ;  // Spin lock;
  }
  EXPECT_EQ("This is", storage.messages);

  MMQ<ProcessableMessage> mmq2;
  ProcessableMessage msg(&storage, " Sparta!");
  mmq2.PushMessage(msg);
  while (storage.count != 3) {
    ;  // Spin lock;
  }
  EXPECT_EQ("This is Sparta!", storage.messages);
}
