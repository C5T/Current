// TODO(dkorolev): Add tests for `Wait`.
// TODO(dkorolev): Run WaitableAtomic tests on Mac.
// TODO(dkorolev): Run WaitableAtomic tests on Windows.

/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#include "waitable_atomic.h"

#include <thread>

#include "../../Bricks/3party/gtest/gtest-main.h"

using std::thread;
using std::this_thread::sleep_for;
using std::chrono::milliseconds;

using sherlock::WaitableAtomic;
using sherlock::IntrusiveClient;

TEST(WaitableAtomic, Smoke) {
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
    thread([&top_level_lock, &object](IntrusiveClient top_level_client) {
             // Should be able to register another client for `top_level_lock`.
             ASSERT_TRUE(bool(top_level_lock.RegisterScopedClient()));
             while (top_level_client) {
               // This loop will be terminated as `top_level_lock` will be leaving the scope.
               ++object.MutableScopedAccessor()->x;
               sleep_for(milliseconds(1));
             }
             // Should no longer be able to register another client for `top_level_lock`.
             ASSERT_FALSE(bool(top_level_lock.RegisterScopedClient()));
             sleep_for(milliseconds(10));
             object.MutableScopedAccessor()->x_done = true;
           },
           top_level_lock.RegisterScopedClient()).detach();

    // The `++y` thread uses the functional style.
    thread([&top_level_lock, &object](IntrusiveClient top_level_client) {
             // Should be able to register another client for `top_level_lock`.
             ASSERT_TRUE(bool(top_level_lock.RegisterScopedClient()));
             while (top_level_client) {
               // This loop will be terminated as `top_level_lock` will be leaving the scope.
               object.MutableUse([](Object& object) { ++object.y; });
               sleep_for(milliseconds(1));
             }
             // Should no longer be able to register another client for `top_level_lock`.
             ASSERT_FALSE(bool(top_level_lock.RegisterScopedClient()));
             sleep_for(milliseconds(10));
             object.MutableUse([](Object& object) { object.y_done = true; });
           },
           top_level_lock.RegisterScopedClient()).detach();

    // Let `++x` and `++y` threads run 25ms.
    sleep_for(milliseconds(25));

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
  EXPECT_GT(copy_of_object.x, 10u);
  EXPECT_LT(copy_of_object.x, 100u);
  EXPECT_GT(copy_of_object.y, 10u);
  EXPECT_LT(copy_of_object.y, 100u);
}

TEST(WaitableAtomic, IntrusiveClientsCanBeTransferred) {
  WaitableAtomic<bool, true> object;
  auto f = [](IntrusiveClient& c) { static_cast<void>(c); };
  thread([&f](IntrusiveClient c) { f(c); }, object.RegisterScopedClient()).detach();
}
