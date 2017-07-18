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

#include "owned_borrowed.h"
#include "waitable_atomic.h"

#include <thread>

#include "../../3rdparty/gtest/gtest-main.h"

// Test using `current::Owned<>` as a `shared_ptr<>`.
TEST(OwnedBorrowed, Owned) {
  current::Owned<int> x1(current::ConstructOwned<int>(), 0);
  EXPECT_EQ(0u, x1.NumberOfActiveBorrowers());
  EXPECT_EQ(0u, x1.TotalBorrowersSpawnedThroughoutLifetime());
  EXPECT_TRUE(static_cast<bool>(x1));
  EXPECT_EQ(0, *x1);
  ++*x1;
  EXPECT_EQ(1, *x1);
  EXPECT_TRUE(static_cast<bool>(x1));
  EXPECT_EQ(0u, x1.NumberOfActiveBorrowers());
  EXPECT_EQ(0u, x1.TotalBorrowersSpawnedThroughoutLifetime());

  current::Owned<int> x2 = std::move(x1);
  EXPECT_EQ(0u, x2.NumberOfActiveBorrowers());
  EXPECT_EQ(0u, x2.TotalBorrowersSpawnedThroughoutLifetime());
  EXPECT_TRUE(static_cast<bool>(x2));
  EXPECT_FALSE(x1.IsValid());
  EXPECT_EQ(1, *x2);

  current::Owned<int> x3(current::ConstructOwned<int>(), 42);
  EXPECT_EQ(0u, x3.NumberOfActiveBorrowers());
  EXPECT_EQ(0u, x3.TotalBorrowersSpawnedThroughoutLifetime());
  EXPECT_TRUE(static_cast<bool>(x3));
  EXPECT_EQ(42, *x3);

  x3 = std::move(x2);
  EXPECT_EQ(0u, x3.NumberOfActiveBorrowers());
  EXPECT_EQ(0u, x3.TotalBorrowersSpawnedThroughoutLifetime());
  EXPECT_TRUE(static_cast<bool>(x3));
  EXPECT_FALSE(static_cast<bool>(x2));
  EXPECT_EQ(1, *x3);

  {
    current::Borrowed<int> y1(x3);
    EXPECT_EQ(1u, x3.NumberOfActiveBorrowers());
    EXPECT_EQ(1u, x3.TotalBorrowersSpawnedThroughoutLifetime());
    EXPECT_TRUE(static_cast<bool>(y1));
    current::Borrowed<int> y1b(std::move(y1));
    EXPECT_FALSE(static_cast<bool>(y1));
    EXPECT_TRUE(static_cast<bool>(y1b));
    EXPECT_EQ(1u, x3.NumberOfActiveBorrowers());
    EXPECT_EQ(1u, x3.TotalBorrowersSpawnedThroughoutLifetime());
    y1 = std::move(y1b);
    EXPECT_TRUE(static_cast<bool>(y1));
    EXPECT_FALSE(static_cast<bool>(y1b));
    EXPECT_EQ(1u, x3.NumberOfActiveBorrowers());
    EXPECT_EQ(1u, x3.TotalBorrowersSpawnedThroughoutLifetime());
  }
  EXPECT_EQ(0u, x3.NumberOfActiveBorrowers());
  EXPECT_EQ(1u, x3.TotalBorrowersSpawnedThroughoutLifetime());

  current::Borrowed<int> y2(x3);
  EXPECT_EQ(1u, x3.NumberOfActiveBorrowers());
  EXPECT_EQ(2u, x3.TotalBorrowersSpawnedThroughoutLifetime());

  x2 = std::move(x3);
  EXPECT_EQ(1u, x2.NumberOfActiveBorrowers());
  EXPECT_EQ(2u, x2.TotalBorrowersSpawnedThroughoutLifetime());
  EXPECT_TRUE(static_cast<bool>(x2));
  EXPECT_FALSE(static_cast<bool>(x3));
  EXPECT_EQ(1, *x2);

  // The following two lines should not compile.
  // Last checked June 2017. -- D.K.
  // current::Owned<int> y1;
  // current::Owned<int> y2(const_cast<const current::Owned<int>&>(x));
}

// Test using `current::Owned<>` via an exclusive mutex-guarded accessor.
TEST(OwnedBorrowed, ExclusiveUseOfOwned) {
  current::Owned<int> x(current::ConstructOwned<int>(), 0);
  x.ExclusiveUse([](int value) { EXPECT_EQ(0, value); });
  x.ExclusiveUse([](int& value) { ++value; });
  x.ExclusiveUse([](int value) { EXPECT_EQ(1, value); });
  EXPECT_EQ(1, *x);  // Confirm the lambdas have been executed synchronously.
}

// Test `current::Owned<>` can be move-constructed.
TEST(OwnedBorrowed, MoveOwned) {
  current::Owned<int> y([]() {
    current::Owned<int> x(current::ConstructOwned<int>(), 0);
    EXPECT_EQ(0, *x);
    ++*x;
    EXPECT_EQ(1, *x);
    return x;
  }());
  EXPECT_EQ(1, *y);
  ++*y;
  EXPECT_EQ(2, *y);
}

// Test `current::BorrowedWithCallback<>` is ref-counted within the master `current::Owned<>`.
TEST(OwnedBorrowed, BorrowedWithCallback) {
  std::string log;

  current::Owned<int> x(current::ConstructOwned<int>(), 0);
  EXPECT_EQ(0u, x.NumberOfActiveBorrowers());
  EXPECT_EQ(0u, x.TotalBorrowersSpawnedThroughoutLifetime());

  {
    current::BorrowedWithCallback<int> y(x, [&log]() { log += "-y\n"; });
    current::BorrowedWithCallback<int> z(x, [&log]() { log += "-z\n"; });
    EXPECT_EQ(2u, x.NumberOfActiveBorrowers());
    EXPECT_EQ(2u, x.TotalBorrowersSpawnedThroughoutLifetime());
    EXPECT_EQ(2u, y.NumberOfActiveBorrowers());
    EXPECT_EQ(2u, y.TotalBorrowersSpawnedThroughoutLifetime());
    EXPECT_EQ(2u, z.NumberOfActiveBorrowers());
    EXPECT_EQ(2u, z.TotalBorrowersSpawnedThroughoutLifetime());
  }
  EXPECT_EQ(0u, x.NumberOfActiveBorrowers());
  EXPECT_EQ(2u, x.TotalBorrowersSpawnedThroughoutLifetime());

  {
    current::BorrowedWithCallback<int> p(x, [&log]() { log += "-p\n"; });
    current::BorrowedWithCallback<int> q(x, [&log]() { log += "-q\n"; });
    EXPECT_EQ(2u, x.NumberOfActiveBorrowers());
    EXPECT_EQ(4u, x.TotalBorrowersSpawnedThroughoutLifetime());
    EXPECT_EQ(2u, p.NumberOfActiveBorrowers());
    EXPECT_EQ(4u, p.TotalBorrowersSpawnedThroughoutLifetime());
    EXPECT_EQ(2u, q.NumberOfActiveBorrowers());
    EXPECT_EQ(4u, q.TotalBorrowersSpawnedThroughoutLifetime());
  }
  EXPECT_EQ(0u, x.NumberOfActiveBorrowers());
  EXPECT_EQ(4u, x.TotalBorrowersSpawnedThroughoutLifetime());

  EXPECT_EQ("", log) << "No lambdas should be called in this test, as `current::BorrowedWithCallback<>` instances go "
                        "out of their respective scopes before the top-level `current::Owned<>` instance does.";
}

// Test the actual scope-ownership logic.
// Create a `current::Owned<>` and use its object from a different thread via `current::BorrowedWithCallback<>`.
// Have the supplementary thread outlive the main one, and confirm the test waits for the supplementary thread
// to complete its job.
TEST(OwnedBorrowed, BorrowedWithCallbackOutlivingTheOwner) {
  // Declare the variable to work with, and a reference wrapper for it, outside the scope of the test.
  // This way the resulting, final value of this variable can be tested as the `Owned/Borrowed` part is done.
  int value = 0;
  struct Container {
    int& ref;
    Container(int& to) : ref(to) {}
  };
  Container container(value);

  std::unique_ptr<std::thread> thread;
  std::string log;
  {
    current::Owned<Container> x(current::ConstructOwned<Container>(), container);

    std::atomic_bool terminating(false);
    // The `unique_ptr` ugliness is to have the pre-initialized `current::BorrowedWithCallback` movable.
    thread = std::make_unique<std::thread>(
        [&log, &terminating](std::unique_ptr<current::BorrowedWithCallback<Container>> y0) {
          current::BorrowedWithCallback<Container>& y = *y0.get();
          EXPECT_TRUE(static_cast<bool>(y));
          // Keep incrementing until external termination request.
          while (!terminating) {
            y.ExclusiveUse([](Container& container) { ++container.ref; });
          }
          EXPECT_FALSE(static_cast<bool>(y));
          // And do another one thousand increments just because we can.
          for (int i = 0; i < 1000; ++i) {
            y.ExclusiveUse([](Container& container) { ++container.ref; });
          }
        },
        std::make_unique<current::BorrowedWithCallback<Container>>(x,
                                                                   [&log, &terminating]() {
                                                                     terminating = true;
                                                                     log += "Terminating.\n";
                                                                   }));

    EXPECT_EQ(1u, x.NumberOfActiveBorrowers());
    EXPECT_EQ(1u, x.TotalBorrowersSpawnedThroughoutLifetime());

    int extracted_value;
    do {
      x.ExclusiveUse([&extracted_value](const Container& container) { extracted_value = container.ref; });
    } while (extracted_value < 10000);

    EXPECT_EQ(1u, x.NumberOfActiveBorrowers());
    EXPECT_EQ(1u, x.TotalBorrowersSpawnedThroughoutLifetime());
  }

  EXPECT_EQ("Terminating.\n", log) << "The client thread must have been signaled to shut down.";
  EXPECT_GE(value, 11000) << "10000 we waited for, plus at least 1000 at the end of the thread.";

  thread->join();
}

// Same as the above test, but do not use an external termination primitive,
// but rely on polling the availability of `current::BorrowedWithCallback<>` itself.
TEST(OwnedBorrowed, UseInternalIsDestructingGetter) {
  int value = 0;
  struct Container {
    int& ref;
    Container(int& to) : ref(to) {}
  };
  Container container(value);

  std::unique_ptr<std::thread> thread;
  {
    current::Owned<Container> x(current::ConstructOwned<Container>(), container);
    thread = std::make_unique<std::thread>([](current::Borrowed<Container> y) {
      // Keep incrementing until external termination request.
      while (y) {
        y.ExclusiveUse([](Container& container) { ++container.ref; });
      }
      // And do another one thousand increments just because we can.
      for (int i = 0; i < 1000; ++i) {
        y.ExclusiveUse([](Container& container) { ++container.ref; });
      }
    }, current::Borrowed<Container>(x));

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
#if !defined(CURRENT_CI) && !defined(CURRENT_COVERAGE_REPORT_MODE)
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
