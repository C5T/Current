/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#include "scope_owned.h"
#include "waitable_atomic.h"

#include <thread>

#include "../../3rdparty/gtest/gtest-main.h"

// Test using `ScopeOwnedByMe<>` as a `shared_ptr<>`.
TEST(ScopeOwned, ScopeOwnedByMe) {
  ScopeOwnedByMe<int> x(0);
  EXPECT_EQ(0, *x);
  ++*x;
  EXPECT_EQ(1, *x);
  EXPECT_TRUE(static_cast<bool>(x));

  // The following two lines should not compile.
  // ScopeOwnedByMe<int> y;
  // ScopeOwnedByMe<int> y(const_cast<const ScopeOwnedByMe<int>&>(x));
}

// Test using `ScopeOwnedByMe<>` via an exclusive mutex-guarded accessor.
TEST(ScopeOwned, ExclusiveUseOfOwnedByMe) {
  ScopeOwnedByMe<int> x(0);
  x.ExclusiveUse([](int value) { EXPECT_EQ(0, value); });
  x.ExclusiveUse([](int& value) { ++value; });
  x.ExclusiveUse([](int value) { EXPECT_EQ(1, value); });
  EXPECT_EQ(1, *x);  // Confirm the lambdas have been executed synchronously.
}

// Test `ScopeOwnedByMe<>` can be move-constructed.
TEST(ScopeOwned, MoveConstructScopeOwnedByMe) {
  ScopeOwnedByMe<int> y([]() {
    ScopeOwnedByMe<int> x(0);
    EXPECT_EQ(0, *x);
    ++*x;
    EXPECT_EQ(1, *x);
    return x;
  }());
  EXPECT_EQ(1, *y);
  ++*y;
  EXPECT_EQ(2, *y);
}

// Test `ScopeOwnedBySomeoneElse<>` is ref-counted within the master `ScopeOwnedByMe<>`.
TEST(ScopeOwned, ScopeOwnedBySomeoneElse) {
  std::string log;

  ScopeOwnedByMe<int> x(0);
  EXPECT_EQ(0u, x.NumberOfActiveFollowers());
  EXPECT_EQ(0u, x.TotalFollowersSpawnedThroughoutLifetime());

  {
    ScopeOwnedBySomeoneElse<int> y(x, [&log]() { log += "-y\n"; });
    ScopeOwnedBySomeoneElse<int> z(x, [&log]() { log += "-z\n"; });
    EXPECT_EQ(2u, x.NumberOfActiveFollowers());
    EXPECT_EQ(2u, x.TotalFollowersSpawnedThroughoutLifetime());
    EXPECT_EQ(2u, y.NumberOfActiveFollowers());
    EXPECT_EQ(2u, y.TotalFollowersSpawnedThroughoutLifetime());
    EXPECT_EQ(2u, z.NumberOfActiveFollowers());
    EXPECT_EQ(2u, z.TotalFollowersSpawnedThroughoutLifetime());
  }
  EXPECT_EQ(0u, x.NumberOfActiveFollowers());
  EXPECT_EQ(2u, x.TotalFollowersSpawnedThroughoutLifetime());

  {
    ScopeOwnedBySomeoneElse<int> p(x, [&log]() { log += "-p\n"; });
    ScopeOwnedBySomeoneElse<int> q(x, [&log]() { log += "-q\n"; });
    EXPECT_EQ(2u, x.NumberOfActiveFollowers());
    EXPECT_EQ(4u, x.TotalFollowersSpawnedThroughoutLifetime());
    EXPECT_EQ(2u, p.NumberOfActiveFollowers());
    EXPECT_EQ(4u, p.TotalFollowersSpawnedThroughoutLifetime());
    EXPECT_EQ(2u, q.NumberOfActiveFollowers());
    EXPECT_EQ(4u, q.TotalFollowersSpawnedThroughoutLifetime());
  }
  EXPECT_EQ(0u, x.NumberOfActiveFollowers());
  EXPECT_EQ(4u, x.TotalFollowersSpawnedThroughoutLifetime());

  EXPECT_EQ("", log) << "No lambdas should be called in this test, as `ScopeOwnedBySomeoneElse<>` instances go "
                        "out of their respective scopes before the top-level `ScopeOwnedByMe<>` instance does.";
}

// Test the actual scope-ownership logic.
// Create a `ScopeOwnedByMe<>` and use its object from a different thread via `ScopeOwnedBySomeoneElse<>`.
// Have the supplementary thread outlive the main one, and confirm the test waits for the supplementary thread
// to complete its job.
TEST(ScopeOwned, ScopeOwnedBySomeoneElseOutlivingTheOwner) {
  // Declare the variable to work with, and a reference wrapper for it, outside the scope of the test.
  // This way the resulting, final value of this variable can be tested as the `ScopeOwned*` part is all done.
  int value = 0;
  struct Container {
    int& ref;
    Container(int& to) : ref(to) {}
  };
  Container container(value);

  std::unique_ptr<std::thread> thread;
  std::string log;
  {
    ScopeOwnedByMe<Container> x(container);

    std::atomic_bool terminating(false);
    thread = std::make_unique<std::thread>(
        [&log, &terminating](ScopeOwnedBySomeoneElse<Container> y) {
          EXPECT_TRUE(static_cast<bool>(y));
          // Keep incrementing until external termination request.
          while (!terminating) {
            y.ExclusiveUseDespitePossiblyDestructing([](Container& container) { ++container.ref; });
          }
          EXPECT_FALSE(static_cast<bool>(y));
          // And do another one thousand increments just because we can.
          for (int i = 0; i < 1000; ++i) {
            y.ExclusiveUseDespitePossiblyDestructing([](Container& container) { ++container.ref; });
          }
        },
        ScopeOwnedBySomeoneElse<Container>(x,
                                           [&log, &terminating]() {
                                             terminating = true;
                                             log += "Terminating.\n";
                                           }));

    EXPECT_EQ(1u, x.NumberOfActiveFollowers());
    EXPECT_EQ(1u, x.TotalFollowersSpawnedThroughoutLifetime());

    int extracted_value;
    do {
      x.ExclusiveUse([&extracted_value](const Container& container) { extracted_value = container.ref; });
    } while (extracted_value < 10000);

    EXPECT_EQ(1u, x.NumberOfActiveFollowers());
    EXPECT_EQ(1u, x.TotalFollowersSpawnedThroughoutLifetime());
  }

  EXPECT_EQ("Terminating.\n", log) << "The client thread must have been signaled to shut down.";
  EXPECT_GE(value, 11000) << "10000 we waited for, plus at least 1000 at the end of the thread.";

  thread->join();
}

// Same as the above test, but do not use an external termination primitive,
// but rely on polling the availability of `ScopeOwnedBySomeoneElse<>` itseld.
TEST(ScopeOwned, UseInternalIsDestructingGetter) {
  int value = 0;
  struct Container {
    int& ref;
    Container(int& to) : ref(to) {}
  };
  Container container(value);

  std::unique_ptr<std::thread> thread;
  {
    ScopeOwnedByMe<Container> x(container);
    thread = std::make_unique<std::thread>([](ScopeOwnedBySomeoneElse<Container> y) {
      // Keep incrementing until external termination request.
      while (!y.IsDestructing()) {
        y.ExclusiveUseDespitePossiblyDestructing([](Container& container) { ++container.ref; });
      }
      // And do another one thousand increments just because we can.
      for (int i = 0; i < 1000; ++i) {
        y.ExclusiveUseDespitePossiblyDestructing([](Container& container) { ++container.ref; });
      }
    }, ScopeOwnedBySomeoneElse<Container>(x, []() {}));

    int extracted_value;
    do {
      x.ExclusiveUse([&extracted_value](const Container& container) { extracted_value = container.ref; });
    } while (extracted_value < 10000);
  }

  EXPECT_GE(value, 11000) << "10000 we waited for, plus at least 1000 at the end of the thread.";
  thread->join();
}

TEST(WaitableAtomic, Smoke) {
  using current::WaitableAtomic;
  using current::IntrusiveClient;

  struct Object {
    size_t x = 0;
    size_t y = 0;
    bool x_done = false;
    bool y_done = false;
  };

  WaitableAtomic<Object> object;
  {
    // This scope runs asynchronous operations in two dedicated threads.
    WaitableAtomic<bool, true> top_level_lock;

    // The `++x` thread uses mutable accessors.
    std::thread([&top_level_lock, &object](IntrusiveClient top_level_client) {
      // Should be able to register another client for `top_level_lock`.
      ASSERT_TRUE(bool(top_level_lock.RegisterScopedClient()));
      while (top_level_client) {
        // This loop will be terminated as `top_level_lock` will be leaving the scope.
        ++object.MutableScopedAccessor()->x;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      // Should no longer be able to register another client for `top_level_lock`.
      ASSERT_FALSE(bool(top_level_lock.RegisterScopedClient()));
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      object.MutableScopedAccessor()->x_done = true;
    }, top_level_lock.RegisterScopedClient()).detach();

    // The `++y` thread uses the functional style.
    std::thread([&top_level_lock, &object](IntrusiveClient top_level_client) {
      // Should be able to register another client for `top_level_lock`.
      ASSERT_TRUE(bool(top_level_lock.RegisterScopedClient()));
      while (top_level_client) {
        // This loop will be terminated as `top_level_lock` will be leaving the scope.
        object.MutableUse([](Object& object) { ++object.y; });
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      // Should no longer be able to register another client for `top_level_lock`.
      ASSERT_FALSE(bool(top_level_lock.RegisterScopedClient()));
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      object.MutableUse([](Object& object) { object.y_done = true; });
    }, top_level_lock.RegisterScopedClient()).detach();

    // Let `++x` and `++y` threads run 25ms.
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    // This block will only finish when both client threads have terminated.
    // This is the reason behind using `.detach()` instead of `.join()`,
    // since the latter would ruin the purpose of the test.
  }

  // Analyze the result.
  Object copy_of_object(object.GetValue());

  // Both threads should have terminated successfully.
  EXPECT_TRUE(copy_of_object.x_done);
  EXPECT_TRUE(copy_of_object.y_done);

  // Both threads should have had enough time to increment their counters at least by a bit.
  // Technically, the EXPECT-s below make the test flaky, but the range is generous enough.
#ifndef CURRENT_COVERAGE_REPORT_MODE
  EXPECT_GT(copy_of_object.x, 10u);
  EXPECT_LT(copy_of_object.x, 100u);
  EXPECT_GT(copy_of_object.y, 10u);
  EXPECT_LT(copy_of_object.y, 100u);
#else
  // Travis is slow these days. No need to make the tests red because of that. -- D.K.
  EXPECT_GT(copy_of_object.x, 5u);
  EXPECT_LT(copy_of_object.x, 500u);
  EXPECT_GT(copy_of_object.y, 5u);
  EXPECT_LT(copy_of_object.y, 500u);
#endif

  // Confirm `[Im]MutableUse(lambda)` proxies the return value of the lambda.
  EXPECT_EQ(copy_of_object.x, object.ImmutableUse([](const Object& object) { return object.x; }));
  EXPECT_EQ(copy_of_object.y + 1u, object.MutableUse([](Object& object) { return ++object.y; }));
}

TEST(WaitableAtomic, ProxyConstructor) {
  using current::WaitableAtomic;

  struct NonDefaultConstructibleObject {
    int a;
    int b;
    NonDefaultConstructibleObject(int a, int b) : a(a), b(b) {}
    int APlusB() const { return a + b; }
  };

  WaitableAtomic<NonDefaultConstructibleObject> object(1, 1);
  EXPECT_EQ(2, object.GetValue().APlusB());
}

TEST(WaitableAtomic, IntrusiveClientsCanBeTransferred) {
  using current::WaitableAtomic;
  using current::IntrusiveClient;

  WaitableAtomic<bool, true> object;
  auto f = [](IntrusiveClient& c) { static_cast<void>(c); };
  std::thread([&f](IntrusiveClient c) { f(c); }, object.RegisterScopedClient()).detach();
}
