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

#define BRICKS_RANDOM_FIX_SEED

#include "clone.h"
#include "make_scope_guard.h"
#include "random.h"
#include "sha256.h"
#include "singleton.h"
#include "waitable_terminate_signal.h"
#include "lazy_instantiation.h"

#include "../exception.h"
#include "../strings/printf.h"

#include "../../3rdparty/gtest/gtest-main.h"

#include <thread>

TEST(Util, BasicException) {
  try {
    BRICKS_THROW(bricks::Exception("Foo"));
    ASSERT_TRUE(false);
  } catch (bricks::Exception& e) {
    // Relative path prefix will be here when measuring code coverage, take it out.
    const std::string actual = e.What();
    const std::string golden = "test.cc:44\tbricks::Exception(\"Foo\")\tFoo";
    ASSERT_GE(actual.length(), golden.length());
    EXPECT_EQ(golden, actual.substr(actual.length() - golden.length()));
  }
}

struct TestException : bricks::Exception {
  TestException(const std::string& a, const std::string& b) : bricks::Exception(a + "&" + b) {}
};

TEST(Util, CustomException) {
  try {
    BRICKS_THROW(TestException("Bar", "Baz"));
    ASSERT_TRUE(false);
  } catch (bricks::Exception& e) {
    // Relative path prefix will be here when measuring code coverage, take it out.
    const std::string actual = e.What();
    const std::string golden = "test.cc:61\tTestException(\"Bar\", \"Baz\")\tBar&Baz";
    ASSERT_GE(actual.length(), golden.length());
    EXPECT_EQ(golden, actual.substr(actual.length() - golden.length()));
  }
}

TEST(Util, MakeScopeGuard) {
  struct Object {
    Object(std::string& story) : story_(story) { story_ += "constructed\n"; }
    ~Object() { story_ += "destructed\n"; }

    Object(const Object&) = delete;
    Object(Object&&) = delete;
    void operator=(const Object&) = delete;
    void operator=(Object&&) = delete;

    std::string& story_;
  };

  {
    std::string story;
    {
      Object object(story);
      EXPECT_EQ("constructed\n", story);
    }
    EXPECT_EQ("constructed\ndestructed\n", story);
  }

  {
    std::string story = "lambda_begin\n";
    EXPECT_EQ("lambda_begin\n", story);
    {
      EXPECT_EQ("lambda_begin\n", story);
      const auto guard = bricks::MakeScopeGuard([&story]() { story += "lambda_end\n"; });
      EXPECT_EQ("lambda_begin\n", story);
    }
    EXPECT_EQ("lambda_begin\nlambda_end\n", story);
  }

  {
    std::string story = "helper_begin\n";
    EXPECT_EQ("helper_begin\n", story);
    struct Helper {
      Helper(std::string& story) : story_(story), called_(false) {}

      Helper(const Helper&) = delete;
      Helper(Helper&&) = delete;
      void operator=(const Helper&) = delete;
      void operator=(Helper&&) = delete;

      void operator()() {
        if (!called_) {
          story_ += "helper_end\n";
          called_ = true;
        } else {
          assert(false);  // LCOV_EXCL_LINE
        }
      }

      std::string& story_;
      std::string dummy_string_;
      bool called_;
    };

    Helper helper(story);
    {
      EXPECT_EQ("helper_begin\n", story);
      const auto guard = bricks::MakeScopeGuard(helper);
      EXPECT_EQ("helper_begin\n", story);
      EXPECT_FALSE(helper.called_);
    }
    EXPECT_EQ("helper_begin\nhelper_end\n", story);
    EXPECT_TRUE(helper.called_);
  }
}

TEST(Util, MakePointerScopeGuard) {
  struct Instance {
    Instance(std::string& story) : story_(story) { story_ += "constructed\n"; }
    ~Instance() { story_ += "destructed\n"; }

    Instance(const Instance&) = delete;
    Instance(Instance&&) = delete;
    void operator=(const Instance&) = delete;
    void operator=(Instance&&) = delete;

    std::string& story_;
  };

  {
    std::string story = "object\n";
    EXPECT_EQ("object\n", story);
    {
      Instance instance(story);
      EXPECT_EQ("object\nconstructed\n", story);
    }
    EXPECT_EQ("object\nconstructed\ndestructed\n", story);
  }

  {
    std::string story = "pointer\n";
    EXPECT_EQ("pointer\n", story);
    {
      Instance* pointer = new Instance(story);
      EXPECT_EQ("pointer\nconstructed\n", story);
      delete pointer;
    }
    EXPECT_EQ("pointer\nconstructed\ndestructed\n", story);
  }

  {
    std::string story = "guarded_pointer\n";
    EXPECT_EQ("guarded_pointer\n", story);
    {
      Instance* pointer = new Instance(story);
      const auto guard = bricks::MakePointerScopeGuard(pointer);
      EXPECT_EQ("guarded_pointer\nconstructed\n", story);
    }
    EXPECT_EQ("guarded_pointer\nconstructed\ndestructed\n", story);
  }

  {
    std::string story = "custom_guarded_pointer\n";
    EXPECT_EQ("custom_guarded_pointer\n", story);
    {
      Instance* pointer = new Instance(story);
      const auto guard = bricks::MakePointerScopeGuard(pointer, [&story](Instance* p) {
        story += "guarded_delete\n";
        delete p;
      });
      EXPECT_EQ("custom_guarded_pointer\nconstructed\n", story);
    }
    EXPECT_EQ("custom_guarded_pointer\nconstructed\nguarded_delete\ndestructed\n", story);
  }
}

TEST(Util, Singleton) {
  struct Foo {
    size_t bar = 0u;
    void baz() { ++bar; }
    void reset() { bar = 0u; }
  };
  EXPECT_EQ(0u, bricks::Singleton<Foo>().bar);
  bricks::Singleton<Foo>().baz();
  EXPECT_EQ(1u, bricks::Singleton<Foo>().bar);
  const auto lambda = []() { bricks::Singleton<Foo>().baz(); };
  EXPECT_EQ(1u, bricks::Singleton<Foo>().bar);
  lambda();
  EXPECT_EQ(2u, bricks::Singleton<Foo>().bar);
  // Allow running the test multiple times, via --gtest_repeat.
  bricks::Singleton<Foo>().reset();
}

TEST(Util, ThreadLocalSingleton) {
  struct Foo {
    size_t bar = 0u;
    void baz() { ++bar; }
  };
  const auto add = [](size_t n) {
    for (size_t i = 0; i < n; ++i) {
      bricks::ThreadLocalSingleton<Foo>().baz();
    }
    EXPECT_EQ(n, bricks::ThreadLocalSingleton<Foo>().bar);
  };
  std::thread t1(add, 50000);
  std::thread t2(add, 10);
  t1.join();
  t2.join();
}

TEST(Util, SHA256) {
  EXPECT_EQ("a591a6d40bf420404a011733cfb7b190d62c65bf0bcda32b57b277d9ad9f146e",
            static_cast<std::string>(bricks::SHA256("Hello World")));
}

#if 0
// Test is disabled since even being initialized with constant seed, random number generator
// returns different values on different platforms :(
TEST(Util, RandomWithFixedSeed) {
  EXPECT_EQ(114, bricks::random::RandomInt(-100, 200));
  EXPECT_EQ(258833541435025064u, bricks::random::RandomUInt64(1e10, 1e18));
  EXPECT_FLOAT_EQ(0.752145, bricks::random::RandomFloat(0.0, 1.0));
  EXPECT_DOUBLE_EQ(-605.7885522709737, bricks::random::RandomDouble(-1024.5, 2048.1));
}
#endif

namespace cloning_unit_test {

// First preference: `.Clone()`.
struct ClonableByRef {
  const std::string text;
  ClonableByRef(const std::string& text = "original") : text(text) {}

  ClonableByRef Clone() const { return ClonableByRef("cloned by ref"); }

  struct Cloner {
    ClonableByRef Clone() { return ClonableByRef("cloned by ptr"); }
  };
  Cloner operator->() const { return Cloner(); }

  ClonableByRef(const ClonableByRef& rhs) : text("copy-constructed from " + rhs.text) {}
};

// Second preference: `->Clone()`.
struct ClonableByPtr {
  const std::string text;
  ClonableByPtr(const std::string& text = "original") : text(text) {}

  struct Cloner {
    Cloner() {}
    ClonableByPtr Clone() const { return ClonableByPtr("cloned by ptr"); }
  };
  const Cloner cloner;
  const Cloner* operator->() const { return &cloner; }

  ClonableByPtr(const ClonableByPtr& rhs) : text("copy-constructed from " + rhs.text) {}
};

// Third preference: copy constructor.
struct ClonableByCtor {
  const std::string text;
  ClonableByCtor(const std::string& text = "original") : text(text) {}
  ClonableByCtor(const ClonableByCtor& rhs) : text("copy-constructed from " + rhs.text) {}
};

// Fourth preference: `ParseJSON<T>(JSON(t))`.
// Inefficient, but it's our shortest shortcut for Cereal-serializable `std::unique_ptr<>`-s. -- D.K.
struct ClonableViaJSON {
  std::string text;
  ClonableViaJSON(const std::string& text = "original") : text(text) {}
  ClonableViaJSON(const ClonableViaJSON&) = delete;
  template <typename A>
  void save(A& ar) const {
    ar(CEREAL_NVP(text));
  }
  template <typename A>
  void load(A& ar) {
    ar(CEREAL_NVP(text));
    text = "deserialized from " + text;
  }
  // Cereal needs this signature to exist to support serializing `ClonableViaJSON`.
  ClonableViaJSON(ClonableViaJSON&&);
};

}  // namespace cloning_unit_test

TEST(Util, Clone) {
  using namespace cloning_unit_test;
  using bricks::Clone;
  using bricks::DefaultCloneFunction;
  using bricks::DefaultCloner;

  EXPECT_EQ("original", ClonableByRef().text);
  EXPECT_EQ("original", ClonableByPtr().text);
  EXPECT_EQ("original", ClonableByCtor().text);
  EXPECT_EQ("original", ClonableViaJSON().text);

  EXPECT_EQ("cloned by ref", Clone(ClonableByRef()).text);
  EXPECT_EQ("cloned by ptr", Clone(ClonableByPtr()).text);
  EXPECT_EQ("copy-constructed from original", Clone(ClonableByCtor()).text);
  EXPECT_EQ("deserialized from original", Clone(ClonableViaJSON()).text);

  EXPECT_EQ("cloned by ref", DefaultCloneFunction<ClonableByRef>()(ClonableByRef()).text);
  EXPECT_EQ("cloned by ptr", DefaultCloneFunction<ClonableByPtr>()(ClonableByPtr()).text);
  EXPECT_EQ("copy-constructed from original", DefaultCloneFunction<ClonableByCtor>()(ClonableByCtor()).text);
  EXPECT_EQ("deserialized from original", DefaultCloneFunction<ClonableViaJSON>()(ClonableViaJSON()).text);

  const auto clone_by_ref = DefaultCloneFunction<ClonableByRef>();
  const auto clone_by_ptr = DefaultCloneFunction<ClonableByPtr>();
  const auto clone_by_ctor = DefaultCloneFunction<ClonableByCtor>();
  const auto clone_via_json = DefaultCloneFunction<ClonableViaJSON>();
  EXPECT_EQ("cloned by ref", clone_by_ref(ClonableByRef()).text);
  EXPECT_EQ("cloned by ptr", clone_by_ptr(ClonableByPtr()).text);
  EXPECT_EQ("copy-constructed from original", clone_by_ctor(ClonableByCtor()).text);
  EXPECT_EQ("deserialized from original", clone_via_json(ClonableViaJSON()).text);

  EXPECT_EQ("deserialized from deserialized from original", Clone(Clone(make_unique<ClonableViaJSON>()))->text);

  EXPECT_EQ("cloned by ref", DefaultCloner::Clone(ClonableByRef()).text);
  EXPECT_EQ("cloned by ptr", DefaultCloner::Clone(ClonableByPtr()).text);
  EXPECT_EQ("copy-constructed from original", DefaultCloner::Clone(ClonableByCtor()).text);
  EXPECT_EQ("deserialized from original", DefaultCloner::Clone(ClonableViaJSON()).text);
}

TEST(Util, WaitableTerminateSignalGotWaitedForEvent) {
  using bricks::WaitableTerminateSignal;

  WaitableTerminateSignal signal;
  size_t counter = 0u;
  std::mutex mutex;
  bool result;
  std::thread thread([&signal, &counter, &result, &mutex]() {
    std::unique_lock<std::mutex> lock(mutex);
    result = signal.WaitUntil(lock, [&counter]() { return counter > 1000u; });
  });

  bool repeat = true;
  while (repeat) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      ++counter;  // Will eventually get to 1000, which the thread is waiting for.
      repeat = (counter < 2000u);
    }
    signal.NotifyOfExternalWaitableEvent();
  }

  thread.join();

  EXPECT_FALSE(result);
  EXPECT_FALSE(signal);
}

TEST(Util, WaitableTerminateSignalGotExternalTerminateSignal) {
  using bricks::WaitableTerminateSignal;

  WaitableTerminateSignal signal;
  size_t counter = 0u;
  std::mutex mutex;
  bool result;
  std::thread thread([&signal, &counter, &mutex, &result]() {
    std::unique_lock<std::mutex> lock(mutex);
    result = signal.WaitUntil(lock, [&counter]() {
      return counter > 1000u;  // Not going to happen in this test.
    });
  });

  bool repeat = true;
  while (repeat) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      ++counter;
      repeat = (counter < 500u);
    }
    signal.NotifyOfExternalWaitableEvent();
  }

  signal.SignalExternalTermination();
  thread.join();

  EXPECT_TRUE(result);
  EXPECT_TRUE(signal);
}

TEST(Util, WaitableTerminateSignalScopedRegisterer) {
  using bricks::WaitableTerminateSignal;
  using bricks::WaitableTerminateSignalBulkNotifier;

  WaitableTerminateSignal signal1;
  WaitableTerminateSignal signal2;
  bool result1;
  bool result2;
  size_t counter = 0u;
  std::mutex mutex;

  std::thread thread1([&signal1, &counter, &mutex, &result1]() {
    std::unique_lock<std::mutex> lock(mutex);
    result1 = signal1.WaitUntil(lock, [&counter]() { return counter > 1000u; });
  });

  std::thread thread2([&signal2, &counter, &mutex, &result2]() {
    std::unique_lock<std::mutex> lock(mutex);
    result2 = signal2.WaitUntil(lock, [&counter]() { return counter > 1000u; });
  });

  WaitableTerminateSignalBulkNotifier bulk;
  WaitableTerminateSignalBulkNotifier::Scope scope1(bulk, signal1);
  WaitableTerminateSignalBulkNotifier::Scope scope2(bulk, signal2);

  bool repeat = true;
  while (repeat) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      ++counter;
      repeat = (counter < 2000u);
    }
    bulk.NotifyAllOfExternalWaitableEvent();
  }

  thread1.join();
  thread2.join();

  EXPECT_FALSE(result1);
  EXPECT_FALSE(signal1);
  EXPECT_FALSE(result2);
  EXPECT_FALSE(signal2);
}

TEST(Util, LazyInstantiation) {
  using bricks::LazilyInstantiated;
  using bricks::DelayedInstantiate;
  using bricks::DelayedInstantiateFromTuple;
  using bricks::DelayedInstantiateWithExtraParameter;
  using bricks::DelayedInstantiateWithExtraParameterFromTuple;

  struct Foo {
    int foo;
    Foo(int foo) : foo(foo) {}
  };

  struct Bar {
    int prefix;
    int bar;
    Bar(int prefix, int bar) : prefix(prefix), bar(bar) {}
    std::string AsString() const { return bricks::strings::Printf("%d:%d", prefix, bar); }
  };

  int v = 2;

  const auto a_1 = DelayedInstantiate<Foo>(1);
  const auto a_2 = DelayedInstantiate<Foo>(v);             // By value.
  const auto a_3 = DelayedInstantiate<Foo>(std::cref(v));  // By reference.

  const auto b_1 = DelayedInstantiateFromTuple<Foo>(std::make_tuple(1));
  const auto b_2 = DelayedInstantiateFromTuple<Foo>(std::make_tuple(v));             // By value.
  const auto b_3 = DelayedInstantiateFromTuple<Foo>(std::make_tuple(std::cref(v)));  // By reference.

  EXPECT_EQ(1, a_1.InstantiateAsSharedPtr()->foo);
  EXPECT_EQ(2, a_2.InstantiateAsSharedPtr()->foo);
  EXPECT_EQ(2, a_3.InstantiateAsSharedPtr()->foo);

  EXPECT_EQ(1, b_1.InstantiateAsSharedPtr()->foo);
  EXPECT_EQ(2, b_2.InstantiateAsSharedPtr()->foo);
  EXPECT_EQ(2, b_3.InstantiateAsSharedPtr()->foo);

  EXPECT_EQ(1, a_1.InstantiateAsUniquePtr()->foo);
  EXPECT_EQ(2, a_2.InstantiateAsUniquePtr()->foo);
  EXPECT_EQ(2, a_3.InstantiateAsUniquePtr()->foo);

  EXPECT_EQ(1, b_1.InstantiateAsUniquePtr()->foo);
  EXPECT_EQ(2, b_2.InstantiateAsUniquePtr()->foo);
  EXPECT_EQ(2, b_3.InstantiateAsUniquePtr()->foo);

  v = 3;

  EXPECT_EQ(1, a_1.InstantiateAsSharedPtr()->foo);
  EXPECT_EQ(2, a_2.InstantiateAsSharedPtr()->foo);
  EXPECT_EQ(3, a_3.InstantiateAsSharedPtr()->foo);

  EXPECT_EQ(1, b_1.InstantiateAsSharedPtr()->foo);
  EXPECT_EQ(2, b_2.InstantiateAsSharedPtr()->foo);
  EXPECT_EQ(3, b_3.InstantiateAsSharedPtr()->foo);

  int q = 0;

  const auto bar_1_q = DelayedInstantiate<Bar>(1, std::ref(q));

  q = 2;
  EXPECT_EQ("1:2", bar_1_q.InstantiateAsSharedPtr()->AsString());
  q = 3;
  EXPECT_EQ("1:3", bar_1_q.InstantiateAsSharedPtr()->AsString());

  const auto bar_x_q = DelayedInstantiateWithExtraParameter<Bar, int>(std::cref(q));
  q = 4;
  EXPECT_EQ("100:4", bar_x_q.InstantiateAsSharedPtrWithExtraParameter(100)->AsString());
  EXPECT_EQ("200:4", bar_x_q.InstantiateAsSharedPtrWithExtraParameter(200)->AsString());
  EXPECT_EQ("300:4", bar_x_q.InstantiateAsUniquePtrWithExtraParameter(300)->AsString());
  EXPECT_EQ("400:4", bar_x_q.InstantiateAsUniquePtrWithExtraParameter(400)->AsString());
  q = 5;
  EXPECT_EQ("100:5", bar_x_q.InstantiateAsSharedPtrWithExtraParameter(100)->AsString());
  EXPECT_EQ("200:5", bar_x_q.InstantiateAsSharedPtrWithExtraParameter(200)->AsString());
  EXPECT_EQ("300:5", bar_x_q.InstantiateAsUniquePtrWithExtraParameter(300)->AsString());
  EXPECT_EQ("400:5", bar_x_q.InstantiateAsUniquePtrWithExtraParameter(400)->AsString());

  const auto bar_y_q = DelayedInstantiateWithExtraParameterFromTuple<Bar, int>(std::make_tuple(std::cref(q)));
  q = 6;
  EXPECT_EQ("100:6", bar_y_q.InstantiateAsSharedPtrWithExtraParameter(100)->AsString());
  EXPECT_EQ("200:6", bar_y_q.InstantiateAsSharedPtrWithExtraParameter(200)->AsString());
  EXPECT_EQ("300:6", bar_y_q.InstantiateAsUniquePtrWithExtraParameter(300)->AsString());
  EXPECT_EQ("400:6", bar_y_q.InstantiateAsUniquePtrWithExtraParameter(400)->AsString());
  q = 7;
  EXPECT_EQ("100:7", bar_y_q.InstantiateAsSharedPtrWithExtraParameter(100)->AsString());
  EXPECT_EQ("200:7", bar_y_q.InstantiateAsSharedPtrWithExtraParameter(200)->AsString());
  EXPECT_EQ("300:7", bar_y_q.InstantiateAsUniquePtrWithExtraParameter(300)->AsString());
  EXPECT_EQ("400:7", bar_y_q.InstantiateAsUniquePtrWithExtraParameter(400)->AsString());
}
