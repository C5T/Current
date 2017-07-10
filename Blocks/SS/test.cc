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

#include "../../TypeSystem/Serialization/json.h"
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
  size_t index = 0u;
  std::deque<DispatchDemoEntry> values;  // Use `deque` to avoid extra copies.

  template <current::locks::MutexLockStatus, typename TIMESTAMP>
  idxts_t PublisherPublishImpl(const DispatchDemoEntry& e, const TIMESTAMP) {
    ++index;
    values.push_back(e);
    return idxts_t(index, current::time::Now());
  }

  template <current::locks::MutexLockStatus, typename TIMESTAMP>
  idxts_t PublisherPublishImpl(DispatchDemoEntry&& e, const TIMESTAMP) {
    ++index;
    values.push_back(std::move(e));
    return idxts_t(index, current::time::Now());
  }

  template <current::locks::MutexLockStatus, typename TIMESTAMP>
  void PublisherUpdateHeadImpl(const TIMESTAMP) const {}
};

}  // namespace stream_system_test

TEST(StreamSystem, EntryPublisher) {
  using namespace stream_system_test;
  using DDE = DispatchDemoEntry;
  using EntryPublisher = current::ss::EntryPublisher<DemoEntryPublisherImpl, DDE>;

  current::time::ResetToZero();

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

namespace ss_unittest {

CURRENT_STRUCT(A) {
  CURRENT_FIELD(a, int);
  CURRENT_CONSTRUCTOR(A)(int a = 0) : a(a) {}
};

CURRENT_STRUCT(B) {
  CURRENT_FIELD(b, int);
  CURRENT_CONSTRUCTOR(B)(int b = 0) : b(b) {}
};

}  // namespace ss_unittest

TEST(StreamSystem, PassEntryToSubscriberIfTypeMatches) {
  using namespace ss_unittest;

  // The fallback wrapper: If the passed in entry type is discarded, return `More`.
  const auto return_more = []() -> current::ss::EntryResponse { return current::ss::EntryResponse::More; };

  struct AcceptorImpl {
    AcceptorImpl() = default;
    AcceptorImpl(const AcceptorImpl&) = delete;
    AcceptorImpl(AcceptorImpl&&) = delete;

    current::ss::EntryResponse operator()(const Variant<A, B>& entry, idxts_t, idxts_t) {
      s = JSON<JSONFormat::Minimalistic>(entry);
      return current::ss::EntryResponse::Done;
    }

    current::ss::EntryResponse operator()(const A& a, idxts_t, idxts_t) {
      s = "A=" + current::ToString(a.a);
      return current::ss::EntryResponse::Done;
    }

    current::ss::EntryResponse operator()(const B& b, idxts_t, idxts_t) {
      s = "B=" + current::ToString(b.b);
      return current::ss::EntryResponse::Done;
    }

    std::string s;
  };

  {
    // Direct calls.
    current::ss::EntrySubscriber<AcceptorImpl, Variant<A, B>> ab;
    static_assert(current::ss::IsEntrySubscriber<decltype(ab), Variant<A, B>>::value, "");
    static_assert(!current::ss::IsEntrySubscriber<decltype(ab), A>::value, "");
    static_assert(!current::ss::IsEntrySubscriber<decltype(ab), B>::value, "");
    EXPECT_EQ(current::ss::EntryResponse::Done, ab(A(1), idxts_t(), idxts_t()));
    EXPECT_EQ("{\"A\":{\"a\":1}}", ab.s);
    EXPECT_EQ(current::ss::EntryResponse::Done, ab(B(2), idxts_t(), idxts_t()));
    EXPECT_EQ("{\"B\":{\"b\":2}}", ab.s);
  }

  {
    // Dispatching of the direct type.
    current::ss::EntrySubscriber<AcceptorImpl, Variant<A, B>> ab;
    static_assert(current::ss::IsEntrySubscriber<decltype(ab), Variant<A, B>>::value, "");
    static_assert(!current::ss::IsEntrySubscriber<decltype(ab), A>::value, "");
    static_assert(!current::ss::IsEntrySubscriber<decltype(ab), B>::value, "");
    EXPECT_EQ(current::ss::EntryResponse::Done,
              (current::ss::PassEntryToSubscriberIfTypeMatches<Variant<A, B>, Variant<A, B>>(
                  ab, return_more, Variant<A, B>(A(3)), idxts_t(), idxts_t())));
    EXPECT_EQ("{\"A\":{\"a\":3}}", ab.s);
    EXPECT_EQ(current::ss::EntryResponse::Done,
              (current::ss::PassEntryToSubscriberIfTypeMatches<Variant<A, B>, Variant<A, B>>(
                  ab, return_more, Variant<A, B>(B(4)), idxts_t(), idxts_t())));
    EXPECT_EQ("{\"B\":{\"b\":4}}", ab.s);
  }

  {
    // Dispatching to a sub-type, A.
    current::ss::EntrySubscriber<AcceptorImpl, A> just_a;
    static_assert(!current::ss::IsEntrySubscriber<decltype(just_a), Variant<A, B>>::value, "");
    static_assert(current::ss::IsEntrySubscriber<decltype(just_a), A>::value, "");
    static_assert(!current::ss::IsEntrySubscriber<decltype(just_a), B>::value, "");
    EXPECT_EQ(current::ss::EntryResponse::Done,
              (current::ss::PassEntryToSubscriberIfTypeMatches<A, Variant<A, B>>(
                  just_a, return_more, Variant<A, B>(A(5)), idxts_t(), idxts_t())));
    EXPECT_EQ("A=5", just_a.s);
    just_a.s = "";
    // Should request `More` on the type that is not accepted, in this case, `B`.
    EXPECT_EQ(current::ss::EntryResponse::More,
              (current::ss::PassEntryToSubscriberIfTypeMatches<A, Variant<A, B>>(
                  just_a, return_more, Variant<A, B>(B(6)), idxts_t(), idxts_t())));
    EXPECT_EQ("", just_a.s);
    // Should request `More` on the type that is not accepted, in this case, an uninitialized `Variant<A, B>`.
    EXPECT_EQ(current::ss::EntryResponse::More,
              (current::ss::PassEntryToSubscriberIfTypeMatches<A, Variant<A, B>>(
                  just_a, return_more, Variant<A, B>(), idxts_t(), idxts_t())));
    EXPECT_EQ("", just_a.s);
  }

  {
    // Dispatching to a sub-type, B.
    current::ss::EntrySubscriber<AcceptorImpl, B> just_b;
    static_assert(!current::ss::IsEntrySubscriber<decltype(just_b), Variant<A, B>>::value, "");
    static_assert(!current::ss::IsEntrySubscriber<decltype(just_b), A>::value, "");
    static_assert(current::ss::IsEntrySubscriber<decltype(just_b), B>::value, "");
    // Should request `More` on the type that is not accepted, in this case, `A`.
    EXPECT_EQ(current::ss::EntryResponse::More,
              (current::ss::PassEntryToSubscriberIfTypeMatches<B, Variant<A, B>>(
                  just_b, return_more, Variant<A, B>(A(7)), idxts_t(), idxts_t())));
    EXPECT_EQ("", just_b.s);
    EXPECT_EQ(current::ss::EntryResponse::Done,
              (current::ss::PassEntryToSubscriberIfTypeMatches<B, Variant<A, B>>(
                  just_b, return_more, Variant<A, B>(B(8)), idxts_t(), idxts_t())));
    EXPECT_EQ("B=8", just_b.s);
    just_b.s = "";
    // Should request `More` on the type that is not accepted, in this case, an uninitialized `Variant<A, B>`.
    EXPECT_EQ(current::ss::EntryResponse::More,
              (current::ss::PassEntryToSubscriberIfTypeMatches<B, Variant<A, B>>(
                  just_b, return_more, Variant<A, B>(), idxts_t(), idxts_t())));
    EXPECT_EQ("", just_b.s);
  }
}
