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

#include "mmq.h"
#include "mmpq.h"

#include <atomic>
#include <chrono>
#include <thread>

#include "../../bricks/strings/printf.h"
#include "../../bricks/strings/join.h"

#include "../../3rdparty/gtest/gtest-main.h"

using current::mmq::MMQ;
using current::mmq::MMPQ;
using current::ss::EntryResponse;

TEST(InMemoryMQ, SmokeTest) {
  current::time::ResetToZero();

  struct ConsumerImpl {
    std::string messages_;
    size_t expected_next_message_index_ = 1u;
    size_t dropped_messages_ = 0u;
    std::atomic_size_t processed_messages_;
    ConsumerImpl() : processed_messages_(0u) {}
    EntryResponse operator()(const std::string& s, idxts_t current, idxts_t) {
      EXPECT_GE(current.index, expected_next_message_index_);
      dropped_messages_ += static_cast<size_t>(current.index - expected_next_message_index_);
      expected_next_message_index_ = static_cast<size_t>(current.index + 1);
      messages_ += s + '\n';
      ++processed_messages_;
      EXPECT_EQ(expected_next_message_index_ - processed_messages_ - 1, dropped_messages_);
      return EntryResponse::More;
    }
  };

  using Consumer = current::ss::EntrySubscriber<ConsumerImpl, std::string>;

  {
    Consumer c;
    MMQ<std::string, Consumer> mmq(c);
    static_assert(current::ss::IsPublisher<decltype(mmq)>::value, "");
    static_assert(current::ss::IsEntryPublisher<decltype(mmq), std::string>::value, "");
    mmq.Publish("one");
    mmq.Publish("two");
    mmq.Publish("three");
    while (c.processed_messages_ != 3) {
      std::this_thread::yield();
    }
    EXPECT_EQ("one\ntwo\nthree\n", c.messages_);
    EXPECT_EQ(0u, c.dropped_messages_);
  }

  {
    Consumer c;
    MMPQ<std::string, Consumer> mmpq(c);
    static_assert(current::ss::IsPublisher<decltype(mmpq)>::value, "");
    static_assert(current::ss::IsEntryPublisher<decltype(mmpq), std::string>::value, "");
    mmpq.Publish("one");
    mmpq.Publish("two");
    mmpq.Publish("three");
    mmpq.UpdateHead();
    while (c.processed_messages_ != 3) {
      std::this_thread::yield();
    }
    EXPECT_EQ("one\ntwo\nthree\n", c.messages_);
    EXPECT_EQ(0u, c.dropped_messages_);
  }
}

struct SuspendableConsumerImpl {
  std::vector<std::string> messages_;
  std::atomic_size_t processed_messages_;
  size_t expected_next_message_index_ = 1u;
  size_t total_messages_accepted_by_the_queue_ = 0u;
  std::atomic_bool suspend_processing_;
  size_t processing_delay_ms_ = 0u;
  SuspendableConsumerImpl() : processed_messages_(0u), suspend_processing_(false) {}
  EntryResponse operator()(const std::string& s, idxts_t current, idxts_t last) {
    EXPECT_EQ(current.index, expected_next_message_index_);
    ++expected_next_message_index_;
    while (suspend_processing_) {
      std::this_thread::yield();
    }
    messages_.push_back(s);
    EXPECT_GE(last.index, total_messages_accepted_by_the_queue_);
    total_messages_accepted_by_the_queue_ = static_cast<size_t>(last.index);
    if (processing_delay_ms_) {
      std::this_thread::sleep_for(std::chrono::milliseconds(processing_delay_ms_));
    }
    ++processed_messages_;
    return EntryResponse::More;
  }
  void SetProcessingDelayMillis(uint64_t delay_ms) { processing_delay_ms_ = static_cast<size_t>(delay_ms); }
};

using SuspendableConsumer = current::ss::EntrySubscriber<SuspendableConsumerImpl, std::string>;

TEST(InMemoryMQ, DropOnOverflowTest) {
  current::time::ResetToZero();

  SuspendableConsumer c;

  // Queue with 10 at most messages in the buffer.
  MMQ<std::string, SuspendableConsumer, 10, true> mmq(c);
  static_assert(current::ss::IsPublisher<decltype(mmq)>::value, "");
  static_assert(current::ss::IsEntryPublisher<decltype(mmq), std::string>::value, "");

  // Suspend the consumer temporarily while the first 25 messages are published.
  c.suspend_processing_ = true;

  // Publish 25 messages, causing an overflow, of which 15 will be discarded.
  size_t messages_accepted = 0u;
  size_t messages_dropped = 0u;
  for (size_t i = 0; i < 25; ++i) {
    if (mmq.Publish(current::strings::Printf("M%02d", static_cast<int>(i))).index) {
      ++messages_accepted;
    } else {
      ++messages_dropped;
    }
  }

  // Confirm that 10/25 messages were accepted, and 15/25 were dropped.
  EXPECT_EQ(10u, messages_accepted);
  EXPECT_EQ(15u, messages_dropped);

  // Eliminate processing delay and wait until the complete queue of 10 messages is played through.
  c.suspend_processing_ = false;
  while (c.processed_messages_ != 10u) {
    std::this_thread::yield();
  }

  // Now, to have the consumer observe the index and the counter of the messages,
  // and note that 15 messages, with 0-based indexes [10 .. 25), have not been seen.
  mmq.Publish("Plus one");
  while (c.processed_messages_ != 11u) {
    std::this_thread::yield();
  }

  // Eleven messages total: ten accepted originally, plus one more published later.
  EXPECT_EQ(c.messages_.size(), 11u);
  EXPECT_EQ(c.total_messages_accepted_by_the_queue_, 11u);
  EXPECT_EQ(c.expected_next_message_index_, 12u);

  // Confirm that 11 messages have reached the consumer: first 10/25 and one more later.
  // Also confirm they are all unique.
  EXPECT_EQ(11u, c.messages_.size());
  EXPECT_EQ(11u, std::set<std::string>(begin(c.messages_), end(c.messages_)).size());
}

TEST(InMemoryMQ, WaitOnOverflowTest) {
  current::time::ResetToZero();

  SuspendableConsumer c;
  c.SetProcessingDelayMillis(1u);

  // Queue with 10 events in the buffer. Don't drop events on overflow.
  MMQ<std::string, SuspendableConsumer, 10, false> mmq(c);
  static_assert(current::ss::IsPublisher<decltype(mmq)>::value, "");
  static_assert(current::ss::IsEntryPublisher<decltype(mmq), std::string>::value, "");

  const auto producer = [&](char prefix, size_t count) {
    for (size_t i = 0; i < count; ++i) {
      mmq.Publish(current::strings::Printf("%c%02d", prefix, static_cast<int>(i)));
    }
  };

  std::vector<std::thread> producers;

  for (size_t i = 0; i < 10; ++i) {
    producers.emplace_back(producer, static_cast<char>('a' + i), 10u);
  }

  for (auto& p : producers) {
    p.join();
  }

  // Since we published 100 messages and the size of the buffer is 10,
  // we must see at least 90 messages processed by this moment.
  EXPECT_GE(c.processed_messages_, 90u);

  // Wait until the rest of the queued messages are processed.
  while (c.processed_messages_ != 100u) {
    std::this_thread::yield();
  }

  // Confirm that none of the messages were dropped.
  EXPECT_EQ(c.processed_messages_, c.total_messages_accepted_by_the_queue_);

  // Ensure that all processed messages are indeed unique.
  std::set<std::string> messages(begin(c.messages_), end(c.messages_));
  EXPECT_EQ(100u, std::set<std::string>(c.messages_.begin(), c.messages_.end()).size());
}

TEST(InMemoryMQ, TimeShouldNotGoBack) {
  current::time::ResetToZero();

  struct ConsumerImpl {
    std::string messages_;
    std::atomic_size_t processed_messages_;
    ConsumerImpl() : processed_messages_(0u) {}
    EntryResponse operator()(const std::string& s, idxts_t, idxts_t) {
      messages_ += s + '\n';
      ++processed_messages_;
      return EntryResponse::More;
    }
  };

  using Consumer = current::ss::EntrySubscriber<ConsumerImpl, std::string>;

  {
    Consumer c;
    MMQ<std::string, Consumer> mmq(c);
    static_assert(current::ss::IsPublisher<decltype(mmq)>::value, "");
    static_assert(current::ss::IsEntryPublisher<decltype(mmq), std::string>::value, "");
    mmq.Publish("one", std::chrono::microseconds(1));
    mmq.Publish("three", std::chrono::microseconds(3));
    ASSERT_THROW(mmq.Publish("two", std::chrono::microseconds(2)), current::ss::InconsistentTimestampException);
    while (c.processed_messages_ != 2) {
      std::this_thread::yield();
    }
    EXPECT_EQ("one\nthree\n", c.messages_);
  }

  {
    Consumer c;
    MMPQ<std::string, Consumer> mmpq(c);
    static_assert(current::ss::IsPublisher<decltype(mmpq)>::value, "");
    static_assert(current::ss::IsEntryPublisher<decltype(mmpq), std::string>::value, "");
    mmpq.Publish("one", std::chrono::microseconds(1));
    mmpq.Publish("three", std::chrono::microseconds(3));
    mmpq.UpdateHead(std::chrono::microseconds(3));
    ASSERT_THROW(mmpq.UpdateHead(std::chrono::microseconds(2)), current::ss::InconsistentTimestampException);
    while (c.processed_messages_ != 2) {
      std::this_thread::yield();
    }
    EXPECT_EQ("one\nthree\n", c.messages_);
  }
}

TEST(InMemoryMQ, MMPQAllowsTimeExplicitlyGoingBack) {
  current::time::ResetToZero();

  struct ConsumerImpl {
    std::vector<std::string> messages_by_indexes_;
    std::vector<std::string> messages_by_timestamps_;
    std::atomic_size_t processed_messages_;
    ConsumerImpl() : processed_messages_(0u) {}
    EntryResponse operator()(const std::string& s, idxts_t idxts, idxts_t) {
      messages_by_indexes_.push_back("[" + current::ToString(idxts.index) + "] = " + s);
      messages_by_timestamps_.push_back(s + " @ " + current::ToString(idxts.us));
      ++processed_messages_;
      return EntryResponse::More;
    }
  };

  using Consumer = current::ss::EntrySubscriber<ConsumerImpl, std::string>;

  Consumer c;
  MMPQ<std::string, Consumer> mmpq(c);
  static_assert(current::ss::IsPublisher<decltype(mmpq)>::value, "");
  static_assert(current::ss::IsEntryPublisher<decltype(mmpq), std::string>::value, "");

  // Publish "one". It gets to the consumer immediately.
  mmpq.Publish("one", std::chrono::microseconds(1));
  mmpq.UpdateHead(std::chrono::microseconds(1));
  while (c.processed_messages_ != 1) {
    std::this_thread::yield();
  }
  EXPECT_EQ("[1] = one", current::strings::Join(c.messages_by_indexes_, ", "));
  EXPECT_EQ("one @ 1", current::strings::Join(c.messages_by_timestamps_, ", "));

  // Publish "two", "three", and "four", in the "wrong" order, to test the `P` part of `MMPQ` does its job.
  mmpq.Publish("three", std::chrono::microseconds(3));
  mmpq.Publish("two", std::chrono::microseconds(2));

  // Push head to `2`, ensuring "one" and "two" (but not "three") are processed.
  mmpq.UpdateHead(std::chrono::microseconds(2));
  while (c.processed_messages_ != 2) {
    std::this_thread::yield();
  }
  EXPECT_EQ("[1] = one, [3] = two", current::strings::Join(c.messages_by_indexes_, ", "));
  EXPECT_EQ("one @ 1, two @ 2", current::strings::Join(c.messages_by_timestamps_, ", "));

  // Publish "four" and push head to `4` to process "three" and "four".
  mmpq.Publish("four", std::chrono::microseconds(4));
  mmpq.UpdateHead(std::chrono::microseconds(4));
  while (c.processed_messages_ != 4) {
    std::this_thread::yield();
  }
  EXPECT_EQ("[1] = one, [3] = two, [2] = three, [4] = four", current::strings::Join(c.messages_by_indexes_, ", "));
  EXPECT_EQ("one @ 1, two @ 2, three @ 3, four @ 4", current::strings::Join(c.messages_by_timestamps_, ", "));
}

TEST(InMemoryMQ, MMPQSupportsUpdateHead) {
  current::time::ResetToZero();

  struct ConsumerImpl {
    std::vector<std::string> messages_by_indexes_;
    std::vector<std::string> messages_by_timestamps_;
    std::atomic_size_t processed_messages_;
    ConsumerImpl() : processed_messages_(0u) {}
    EntryResponse operator()(const std::string& s, idxts_t idxts, idxts_t) {
      messages_by_indexes_.push_back("[" + current::ToString(idxts.index) + "] = " + s);
      messages_by_timestamps_.push_back(s + " @ " + current::ToString(idxts.us));
      ++processed_messages_;
      return EntryResponse::More;
    }
  };

  using Consumer = current::ss::EntrySubscriber<ConsumerImpl, std::string>;

  Consumer c;
  MMPQ<std::string, Consumer> mmpq(c);
  static_assert(current::ss::IsPublisher<decltype(mmpq)>::value, "");
  static_assert(current::ss::IsEntryPublisher<decltype(mmpq), std::string>::value, "");

  // Start with "three": publish and confirm it goes through.
  mmpq.Publish("three", std::chrono::microseconds(3));
  mmpq.UpdateHead(std::chrono::microseconds(3));
  while (c.processed_messages_ != 1) {
    std::this_thread::yield();
  }
  EXPECT_EQ("[1] = three", current::strings::Join(c.messages_by_indexes_, ", "));
  EXPECT_EQ("three @ 3", current::strings::Join(c.messages_by_timestamps_, ", "));

  // Follow up with "seven". Without `UpdateHead()` it won't get to the consumer. With `UpdateHead()` it does.
  mmpq.Publish("seven", std::chrono::microseconds(7));
  mmpq.UpdateHead(std::chrono::microseconds(7));
  while (c.processed_messages_ != 2) {
    std::this_thread::yield();
  }
  EXPECT_EQ("[1] = three, [2] = seven", current::strings::Join(c.messages_by_indexes_, ", "));
  EXPECT_EQ("three @ 3, seven @ 7", current::strings::Join(c.messages_by_timestamps_, ", "));

  // Can't update head to anything prior and including `7`, as it is already at `7`.
  ASSERT_THROW(mmpq.UpdateHead(std::chrono::microseconds(5)), current::ss::InconsistentTimestampException);
  ASSERT_THROW(mmpq.UpdateHead(std::chrono::microseconds(7)), current::ss::InconsistentTimestampException);

  // Confirm `UpdateHead()` prevents publishing into the past compared to itself as well.
  // Here, as HEAD is bumped to `21`, publishing "eleven", as well as "blackjack" should correctly fail.
  mmpq.UpdateHead(std::chrono::microseconds(25));
  ASSERT_THROW(mmpq.UpdateHead(std::chrono::microseconds(11)), current::ss::InconsistentTimestampException);
  ASSERT_THROW(mmpq.UpdateHead(std::chrono::microseconds(21)), current::ss::InconsistentTimestampException);

  // Publish an "ace" at 100.
  mmpq.Publish("ace", std::chrono::microseconds(100));
  mmpq.UpdateHead(std::chrono::microseconds(100));
  while (c.processed_messages_ != 3) {
    std::this_thread::yield();
  }
  EXPECT_EQ("[1] = three, [2] = seven, [3] = ace", current::strings::Join(c.messages_by_indexes_, ", "));
  EXPECT_EQ("three @ 3, seven @ 7, ace @ 100", current::strings::Join(c.messages_by_timestamps_, ", "));

  // Now, at `t=100`, publish three more messages at {101,102,103}, all into the future, in a mixed up order.
  mmpq.Publish("queen", std::chrono::microseconds(102));
  mmpq.Publish("king", std::chrono::microseconds(101));
  mmpq.Publish("jack", std::chrono::microseconds(103));

  // Update HEAD to capture just one of the three, to `t=101`, and confirm "king" does get to the consumer.
  mmpq.UpdateHead(std::chrono::microseconds(101));
  while (c.processed_messages_ != 4) {
    std::this_thread::yield();
  }
  EXPECT_EQ("[1] = three, [2] = seven, [3] = ace, [5] = king", current::strings::Join(c.messages_by_indexes_, ", "));
  EXPECT_EQ("three @ 3, seven @ 7, ace @ 100, king @ 101", current::strings::Join(c.messages_by_timestamps_, ", "));

  // Update HEAD to capture the second one of three, to `t=102`, and confirm "queen" does get to the consumer.
  mmpq.UpdateHead(std::chrono::microseconds(102));
  while (c.processed_messages_ != 5) {
    std::this_thread::yield();
  }
  EXPECT_EQ("[1] = three, [2] = seven, [3] = ace, [5] = king, [4] = queen",
            current::strings::Join(c.messages_by_indexes_, ", "));
  EXPECT_EQ("three @ 3, seven @ 7, ace @ 100, king @ 101, queen @ 102",
            current::strings::Join(c.messages_by_timestamps_, ", "));

  // Finally, publish "joker", in the "present" which is way ahead in time compared to the third one left, the "jack".
  mmpq.Publish("joker", std::chrono::microseconds(1000));
  mmpq.UpdateHead(std::chrono::microseconds(1000));
  while (c.processed_messages_ != 7) {
    std::this_thread::yield();
  }
  EXPECT_EQ("[1] = three, [2] = seven, [3] = ace, [5] = king, [4] = queen, [6] = jack, [7] = joker",
            current::strings::Join(c.messages_by_indexes_, ", "));
  EXPECT_EQ("three @ 3, seven @ 7, ace @ 100, king @ 101, queen @ 102, jack @ 103, joker @ 1000",
            current::strings::Join(c.messages_by_timestamps_, ", "));
}
