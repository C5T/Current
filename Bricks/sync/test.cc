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

#include <thread>

#include "../../3rdparty/gtest/gtest-main.h"

// Test using `ScopeOwnedByMe<>` as a `shared_ptr<>`.
TEST(ScopeOwned, ScopeOwnedByMe) {
  ScopeOwnedByMe<int> x(0);
  EXPECT_EQ(0, *x);
  ++*x;
  EXPECT_EQ(1, *x);

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
          // Keep incrementing until external termination request.
          while (!terminating) {
            y.ExclusiveUseDespitePossiblyDestructing([](Container& container) { ++container.ref; });
          }
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
