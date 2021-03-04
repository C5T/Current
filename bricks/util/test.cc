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

#include "accumulative_scoped_deleter.h"
#include "base64.h"
#include "comparators.h"
#include "crc32.h"
#include "iterator.h"
#include "lazy_instantiation.h"
#include "make_scope_guard.h"
#include "random.h"
#include "rol.h"
#include "sha256.h"
#include "singleton.h"
#include "waitable_terminate_signal.h"
#include "object_interface.h"

#include "../exception.h"
#include "../file/file.h"
#include "../strings/printf.h"

#include "../../3rdparty/gtest/gtest-main.h"

#include <thread>

TEST(Util, BasicException) {
  int exception_line = 0;
  try {
    exception_line = __LINE__ + 1;
    CURRENT_THROW(current::Exception("Foo"));
    ASSERT_TRUE(false);
  } catch (current::Exception& e) {
    EXPECT_EQ("current::Exception(\"Foo\")", e.Caller());
    EXPECT_EQ("Foo", e.OriginalDescription());
    EXPECT_EQ(exception_line, e.Line());
  }
}

struct TestException : current::Exception {
  TestException(const std::string& a, const std::string& b) : current::Exception(a + "&" + b) {}
};

TEST(Util, CustomException) {
  int exception_line = 0;
  try {
    exception_line = __LINE__ + 1;
    CURRENT_THROW(TestException("Bar", "Baz"));
    ASSERT_TRUE(false);
  } catch (current::Exception& e) {
    EXPECT_EQ("TestException(\"Bar\", \"Baz\")", e.Caller());
    EXPECT_EQ("Bar&Baz", e.OriginalDescription());
    EXPECT_EQ(exception_line, e.Line());
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
      const auto guard = current::MakeScopeGuard([&story]() { story += "lambda_end\n"; });
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
          CURRENT_ASSERT(false);  // LCOV_EXCL_LINE
        }
      }

      std::string& story_;
      std::string dummy_string_;
      bool called_;
    };

    Helper helper(story);
    {
      EXPECT_EQ("helper_begin\n", story);
      const auto guard = current::MakeScopeGuard(helper);
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
      const auto guard = current::MakePointerScopeGuard(pointer);
      EXPECT_EQ("guarded_pointer\nconstructed\n", story);
    }
    EXPECT_EQ("guarded_pointer\nconstructed\ndestructed\n", story);
  }

  {
    std::string story = "custom_guarded_pointer\n";
    EXPECT_EQ("custom_guarded_pointer\n", story);
    {
      Instance* pointer = new Instance(story);
      const auto guard = current::MakePointerScopeGuard(pointer,
                                                        [&story](Instance* p) {
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
  EXPECT_EQ(0u, current::Singleton<Foo>().bar);
  current::Singleton<Foo>().baz();
  EXPECT_EQ(1u, current::Singleton<Foo>().bar);
  const auto lambda = []() { current::Singleton<Foo>().baz(); };
  EXPECT_EQ(1u, current::Singleton<Foo>().bar);
  lambda();
  EXPECT_EQ(2u, current::Singleton<Foo>().bar);
  // Allow running the test multiple times, via --gtest_repeat.
  current::Singleton<Foo>().reset();
}

TEST(Util, ThreadLocalSingleton) {
  struct Foo {
    size_t bar = 0u;
    void baz() { ++bar; }
  };
  const auto add = [](size_t n) {
    for (size_t i = 0; i < n; ++i) {
      current::ThreadLocalSingleton<Foo>().baz();
    }
    EXPECT_EQ(n, current::ThreadLocalSingleton<Foo>().bar);
  };
  std::thread t1(add, 50000);
  std::thread t2(add, 10);
  t1.join();
  t2.join();
}

TEST(Util, Base64) {
  using current::Base64Encode;
  using current::Base64URLEncode;
  using current::Base64Decode;
  using current::Base64URLDecode;

  EXPECT_EQ("", Base64Encode(""));
  EXPECT_EQ("Zg==", Base64Encode("f"));
  EXPECT_EQ("Zm8=", Base64Encode("fo"));
  EXPECT_EQ("Zm9v", Base64Encode("foo"));
  EXPECT_EQ("Zm9vYg==", Base64Encode("foob"));
  EXPECT_EQ("Zm9vYmE=", Base64Encode("fooba"));
  EXPECT_EQ("Zm9vYmFy", Base64Encode("foobar"));
  EXPECT_EQ("MDw+", Base64Encode("0<>"));
  EXPECT_EQ("MDw-", Base64URLEncode("0<>"));

  EXPECT_EQ("", Base64Decode(""));
  EXPECT_EQ("f", Base64Decode("Zg=="));
  EXPECT_EQ("fo", Base64Decode("Zm8="));
  EXPECT_EQ("foo", Base64Decode("Zm9v"));
  EXPECT_EQ("foob", Base64Decode("Zm9vYg=="));
  EXPECT_EQ("fooba", Base64Decode("Zm9vYmE="));
  EXPECT_EQ("foobar", Base64Decode("Zm9vYmFy"));
  EXPECT_EQ("0<>", Base64Decode("MDw+"));
  EXPECT_EQ("0<>", Base64URLDecode("MDw-"));

  EXPECT_EQ("fw==", Base64Encode("\x7f"));
  EXPECT_EQ("gA==", Base64Encode("\x80"));

  EXPECT_EQ("\x7f", Base64Decode("fw=="));
  EXPECT_EQ("\x80", Base64Decode("gA=="));

  std::string all_chars;
  all_chars.resize(256);
  for (int i = 0; i < 256; ++i) {
    all_chars[i] = char(i);
  }
  EXPECT_EQ(all_chars, Base64Decode(Base64Encode(all_chars)));
  EXPECT_EQ(all_chars, Base64URLDecode(Base64URLEncode(all_chars)));

#ifndef NDEBUG
  // The very `Base64DecodeException` is debug mode only.
  EXPECT_THROW(Base64Decode("MDw-"), current::Base64DecodeException);
  EXPECT_THROW(Base64URLDecode("MDw+"), current::Base64DecodeException);
#endif

  const std::string golden_file = current::FileSystem::ReadFileAsString("golden/base64test.txt");
  EXPECT_EQ(golden_file, Base64Decode(current::FileSystem::ReadFileAsString("golden/base64test.base64")));
  EXPECT_EQ(golden_file, Base64URLDecode(current::FileSystem::ReadFileAsString("golden/base64test.base64url")));
}

TEST(Util, CRC32) {
  const std::string test_string = "Test string";
  EXPECT_EQ(2514197138u, current::CRC32(test_string));
  EXPECT_EQ(2514197138u, current::CRC32(test_string.c_str()));
}

TEST(Util, SHA256) {
  EXPECT_EQ("a591a6d40bf420404a011733cfb7b190d62c65bf0bcda32b57b277d9ad9f146e",
            static_cast<std::string>(current::SHA256("Hello World")));
}

TEST(Util, ROL64) {
  EXPECT_EQ(0x1ull, current::ROL64(1, 0));
  EXPECT_EQ(0x10ull, current::ROL64(1, 4));
  EXPECT_EQ(0x100ull, current::ROL64(1, 8));

  EXPECT_EQ(0x42ull, current::ROL64(0x42, 0));
  EXPECT_EQ(0x420ull, current::ROL64(0x42, 4));
  EXPECT_EQ(0x4200ull, current::ROL64(0x42, 8));

  EXPECT_EQ(0x1ull, current::ROL64(0x10, -4));
  EXPECT_EQ(0x1ull, current::ROL64(0x10, 64 - 4));

  EXPECT_EQ(static_cast<uint64_t>(std::pow(2.0, 63)), current::ROL64(1, 63));
  EXPECT_EQ(1ull, current::ROL64(static_cast<uint64_t>(std::pow(2.0, 63)), 1));
}

#if 0
// This test is disabled since even being initialized with constant seed,
// the random number generator returns different values on different platforms. :(
TEST(Util, RandomWithFixedSeed) {
  EXPECT_EQ(114, current::random::RandomInt(-100, 200));
  EXPECT_EQ(258833541435025064u, current::random::RandomUInt64(1e10, 1e18));
  EXPECT_FLOAT_EQ(0.752145, current::random::RandomFloat(0.0, 1.0));
  EXPECT_DOUBLE_EQ(-605.7885522709737, current::random::RandomDouble(-1024.5, 2048.1));
}
#endif

TEST(Util, WaitableTerminateSignalGotWaitedForEvent) {
  using current::WaitableTerminateSignal;

  WaitableTerminateSignal signal;
  size_t counter = 0u;
  std::mutex mutex;
  bool result_set = false;
  bool result;
  std::thread thread([&signal, &counter, &result, &result_set, &mutex]() {
    std::unique_lock<std::mutex> lock(mutex);
    result = signal.WaitUntil(lock, [&counter]() { return counter > 1000u; });
    result_set = true;
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

  EXPECT_TRUE(result_set);
  EXPECT_FALSE(result);
  EXPECT_FALSE(signal);
}

TEST(Util, WaitableTerminateSignalGotExternalTerminateSignal) {
  using current::WaitableTerminateSignal;

  WaitableTerminateSignal signal;
  size_t counter = 0u;
  std::mutex mutex;
  bool result_set = false;
  bool result;
  std::thread thread([&signal, &counter, &mutex, &result, &result_set]() {
    std::unique_lock<std::mutex> lock(mutex);
    result = signal.WaitUntil(lock,
                              [&counter]() {
                                return counter > 1000u;  // Not going to happen in this test.
                              });
    result_set = true;
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

  {
    std::lock_guard<std::mutex> lock(mutex);
    signal.SignalExternalTermination();
  }

  thread.join();

  EXPECT_TRUE(result_set);
  EXPECT_TRUE(result);
  EXPECT_TRUE(signal);
}

TEST(Util, WaitableTerminateSignalScopedRegisterer) {
  using current::WaitableTerminateSignal;
  using current::WaitableTerminateSignalBulkNotifier;

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
  using current::LazilyInstantiated;
  using current::DelayedInstantiate;
  using current::DelayedInstantiateFromTuple;
  using current::DelayedInstantiateWithExtraParameter;
  using current::DelayedInstantiateWithExtraParameterFromTuple;

  struct Foo {
    int foo;
    Foo(int foo) : foo(foo) {}
  };

  struct Bar {
    int prefix;
    int bar;
    Bar(int prefix, int bar) : prefix(prefix), bar(bar) {}
    std::string AsString() const { return current::strings::Printf("%d:%d", prefix, bar); }
  };

  int v = 2;

  const auto a_1 = DelayedInstantiate<Foo>(1);
  const auto a_2 = DelayedInstantiate<Foo>(v);             // By value.
  const auto a_3 = DelayedInstantiate<Foo>(std::cref(v));  // By reference.

  static_assert(std::is_same_v<typename decltype(a_1)::lazily_instantiated_t, Foo>);
  static_assert(std::is_same_v<typename decltype(a_2)::lazily_instantiated_t, Foo>);
  static_assert(std::is_same_v<typename decltype(a_3)::lazily_instantiated_t, Foo>);

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
  static_assert(std::is_same_v<typename decltype(bar_1_q)::lazily_instantiated_t, Bar>);

  q = 2;
  EXPECT_EQ("1:2", bar_1_q.InstantiateAsSharedPtr()->AsString());
  q = 3;
  EXPECT_EQ("1:3", bar_1_q.InstantiateAsSharedPtr()->AsString());

  const auto bar_x_q = DelayedInstantiateWithExtraParameter<Bar, int>(std::cref(q));
  static_assert(std::is_same_v<typename decltype(bar_x_q)::lazily_instantiated_t, Bar>);

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
  static_assert(std::is_same_v<typename decltype(bar_y_q)::lazily_instantiated_t, Bar>);

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

TEST(Util, GenericMapAccessor) {
  using map1_t = std::map<int, std::string>;
  map1_t map1;

  using accessor1_t = current::GenericMapAccessor<map1_t>;
  const auto accessor1 = accessor1_t(map1);
  EXPECT_TRUE(accessor1.Empty());
  EXPECT_EQ(0u, accessor1.Size());
  EXPECT_TRUE(accessor1.begin() == accessor1.end());
  EXPECT_TRUE(accessor1.rbegin() == accessor1.rend());
  EXPECT_EQ(0, std::distance(accessor1.begin(), accessor1.end()));
  EXPECT_EQ(0, std::distance(accessor1.rbegin(), accessor1.rend()));
  for (char c = 'a'; c <= 'z'; ++c) {
    map1[c - 'a'] = c;
  }
  EXPECT_FALSE(accessor1.Empty());
  EXPECT_EQ(26u, accessor1.Size());
  EXPECT_FALSE(accessor1.begin() == accessor1.end());
  EXPECT_FALSE(accessor1.rbegin() == accessor1.rend());
  EXPECT_EQ(26, std::distance(accessor1.begin(), accessor1.end()));
  EXPECT_EQ(26, std::distance(accessor1.rbegin(), accessor1.rend()));
  // Test forward iterators.
  std::string forward_str1;
  for (const auto& element : accessor1) {
    forward_str1 += element;
  }
  EXPECT_EQ("abcdefghijklmnopqrstuvwxyz", forward_str1);
  // Test reverse iterators.
  std::string reverse_str1;
  for (auto cit = accessor1.rbegin(); cit != accessor1.rend(); ++cit) {
    reverse_str1 += *cit;
  }
  EXPECT_EQ("zyxwvutsrqponmlkjihgfedcba", reverse_str1);
  // Test `{Upper/Lower}Bound`.
  EXPECT_EQ(25, accessor1.UpperBound(24).key());
  EXPECT_EQ("z", *accessor1.UpperBound(24));
  EXPECT_TRUE(accessor1.UpperBound(25) == accessor1.end());
  EXPECT_EQ(1, accessor1.LowerBound(1).key());
  EXPECT_EQ("b", *accessor1.LowerBound(1));
  EXPECT_TRUE(accessor1.LowerBound(0) == accessor1.begin());
  // Test construction of reverse iterator from forward iterator.
  using rit1_t = typename accessor1_t::reverse_iterator_t;
  EXPECT_TRUE(rit1_t(accessor1.end()) == accessor1.rbegin());
  EXPECT_TRUE(rit1_t(accessor1.begin()) == accessor1.rend());

  using map2_t = std::map<int, std::unique_ptr<char>>;
  map2_t map2;
  using accessor2_t = current::GenericMapAccessor<map2_t>;
  const auto accessor2 = accessor2_t(map2);
  EXPECT_TRUE(accessor2.Empty());
  EXPECT_EQ(0u, accessor2.Size());
  EXPECT_TRUE(accessor2.begin() == accessor2.end());
  EXPECT_TRUE(accessor2.rbegin() == accessor2.rend());
  EXPECT_EQ(0, std::distance(accessor2.begin(), accessor2.end()));
  EXPECT_EQ(0, std::distance(accessor2.rbegin(), accessor2.rend()));
  for (auto it = accessor1.begin(), eit = accessor1.end(); it != eit; ++it) {
    map2[it.key()] = std::make_unique<char>((*it)[0] - 'a' + 'A');
  }
  EXPECT_FALSE(accessor2.Empty());
  EXPECT_EQ(26u, accessor2.Size());
  EXPECT_FALSE(accessor2.begin() == accessor2.end());
  EXPECT_FALSE(accessor2.rbegin() == accessor2.rend());
  EXPECT_EQ(26, std::distance(accessor2.begin(), accessor2.end()));
  EXPECT_EQ(26, std::distance(accessor2.rbegin(), accessor2.rend()));
  // Test forward iterators.
  std::string forward_str2;
  for (const auto& element : accessor2) {
    forward_str2 += element;
  }
  EXPECT_EQ("ABCDEFGHIJKLMNOPQRSTUVWXYZ", forward_str2);
  // Test reverse iterators.
  std::string reverse_str2;
  for (auto cit = accessor2.rbegin(); cit != accessor2.rend(); ++cit) {
    reverse_str2 += *cit;
  }
  EXPECT_EQ("ZYXWVUTSRQPONMLKJIHGFEDCBA", reverse_str2);
  // Test `{Upper/Lower}Bound`.
  EXPECT_EQ(25, accessor2.UpperBound(24).key());
  EXPECT_EQ('Z', *accessor2.UpperBound(24));
  EXPECT_TRUE(accessor2.UpperBound(25) == accessor2.end());
  EXPECT_EQ(1, accessor2.LowerBound(1).key());
  EXPECT_EQ('B', *accessor2.LowerBound(1));
  EXPECT_TRUE(accessor2.LowerBound(0) == accessor2.begin());
  // Test construction of reverse iterator from forward iterator.
  using rit2_t = typename accessor2_t::reverse_iterator_t;
  EXPECT_TRUE(rit2_t(accessor2.end()) == accessor2.rbegin());
  EXPECT_TRUE(rit2_t(accessor2.begin()) == accessor2.rend());
}

TEST(AccumulativeScopedDeleter, Smoke) {
  using current::AccumulativeScopedDeleter;

  std::string tracker;
  {
    AccumulativeScopedDeleter<void> deleter;
    deleter += AccumulativeScopedDeleter<void>([&tracker]() { tracker += 'a'; });
    EXPECT_EQ("", tracker);
  }
  EXPECT_EQ("a", tracker);
}

TEST(AccumulativeScopedDeleter, MovesAway) {
  using current::AccumulativeScopedDeleter;

  std::string tracker;
  {
    AccumulativeScopedDeleter<void> top_level_deleter;
    {
      AccumulativeScopedDeleter<void> deleter;
      deleter += AccumulativeScopedDeleter<void>([&tracker]() { tracker += 'b'; });
      EXPECT_EQ("", tracker);
      top_level_deleter = std::move(deleter);
      EXPECT_EQ("", tracker);
    }
    EXPECT_EQ("", tracker);
  }
  EXPECT_EQ("b", tracker);
}

TEST(AccumulativeScopedDeleter, RegistersMultiple) {
  using current::AccumulativeScopedDeleter;

  std::string tracker;
  {
    AccumulativeScopedDeleter<void> top_level_deleter;
    {
      AccumulativeScopedDeleter<void> deleter;
      deleter += AccumulativeScopedDeleter<void>([&tracker]() { tracker += 'c'; });
      deleter += AccumulativeScopedDeleter<void>([&tracker]() { tracker += 'd'; }) +
                 (AccumulativeScopedDeleter<void>([&tracker]() { tracker += 'e'; }) +
                  AccumulativeScopedDeleter<void>([&tracker]() { tracker += 'f'; }));
      EXPECT_EQ("", tracker);
      top_level_deleter = std::move(deleter);
      EXPECT_EQ("", tracker);
    }
    EXPECT_EQ("", tracker);
  }
  EXPECT_EQ("fedc", tracker);
}

TEST(AccumulativeScopedDeleter, DoesNotDeleteWhatShouldStay) {
  using current::AccumulativeScopedDeleter;

  {
    std::string tracker;
    {
      AccumulativeScopedDeleter<void, false>([&tracker]() { tracker += 'b'; });
      EXPECT_EQ("", tracker);
    }
    EXPECT_EQ("", tracker);
  }

  {
    std::string tracker;
    AccumulativeScopedDeleter<void, false>([&tracker]() { tracker += 'c'; }) +
        AccumulativeScopedDeleter<void, false>([&tracker]() { tracker += 'd'; });
    EXPECT_EQ("", tracker);
  }

  {
    // Initializing a real, AccumulativeScopedDeleter<>, object does invoke the deleter.
    std::string tracker;
    {
      AccumulativeScopedDeleter<void> scope = AccumulativeScopedDeleter<void, false>([&tracker]() { tracker += 'e'; });
      EXPECT_EQ("", tracker);
    }
    EXPECT_EQ("e", tracker);
  }

  {
    // Initializing a real, AccumulativeScopedDeleter<>, object via `operator=` does invoke the deleter.
    std::string tracker;
    {
      AccumulativeScopedDeleter<void> scope;
      scope = AccumulativeScopedDeleter<void, false>([&tracker]() { tracker += 'f'; });
      EXPECT_EQ("", tracker);
    }
    EXPECT_EQ("f", tracker);
  }

  {
    std::string tracker;
    const auto f = [&tracker]() { return AccumulativeScopedDeleter<void, false>([&tracker]() { tracker += 'g'; }); };
    {
      // Just returning another object from a function does not invoke the deleter.
      {
        f();
        EXPECT_EQ("", tracker);
      }
      EXPECT_EQ("", tracker);
    }
    {
      // Storing the object returned from the function does invoke the deleter.
      {
        AccumulativeScopedDeleter<void> scope = f();
        EXPECT_EQ("", tracker);
      }
      EXPECT_EQ("g", tracker);
    }
  }
}

struct WithoutHashFunctionTestStruct {};

struct WithHashFunctionTestStruct {
  size_t Hash() const { return 2; }
};

namespace std {
template <>
struct hash<WithoutHashFunctionTestStruct> {
  std::size_t operator()(const WithoutHashFunctionTestStruct&) const { return 1; }
};
template <>
struct hash<WithHashFunctionTestStruct> {
  std::size_t operator()(const WithHashFunctionTestStruct&) const { return 1; }
};
}  // namespace std

TEST(CustomHashFunction, Smoke) {
  using current::GenericHashFunction;

  EXPECT_EQ(1u, std::hash<WithoutHashFunctionTestStruct>()(WithoutHashFunctionTestStruct()));
  EXPECT_EQ(1u, std::hash<WithHashFunctionTestStruct>()(WithHashFunctionTestStruct()));

  EXPECT_EQ(1u, GenericHashFunction<WithoutHashFunctionTestStruct>()(WithoutHashFunctionTestStruct()));
  EXPECT_EQ(2u, GenericHashFunction<WithHashFunctionTestStruct>()(WithHashFunctionTestStruct()));
}

struct ObjectWithInterface {
  int calls = 0;
  int total_calls() { return ++calls; }
  static int add(int a, int b) { return a + b; }
};

CURRENT_OBJECT_INTERFACE(ObjectWithInterface) {
  CURRENT_FORWARD_METHOD(total_calls);
  CURRENT_FORWARD_METHOD(add);
};

template <typename T>
struct ObjectContainer {
  T object;
  template <typename... ARGS>
  ObjectContainer(ARGS&&... args)
      : object(std::forward<ARGS>(args)...) {}
};

template <typename T>
struct ExampleObjectInterface : private ObjectContainer<T>, public CURRENT_OBJECT_INTERFACE_METHODS_WRAPPER<T> {
  using object_container_t = ObjectContainer<T>;
  using methods_wrapper_t = CURRENT_OBJECT_INTERFACE_METHODS_WRAPPER<T>;

  template <typename... ARGS>
  ExampleObjectInterface(ARGS&&... args)
      : object_container_t(std::forward<ARGS>(args)...) {
    methods_wrapper_t::CURRENT_IMPLEMENTATION_POINTER = &(object_container_t::object);
  }
};

namespace object_with_interface_test_namespace {

struct NamespacedObjectWithInterface {
  int calls = 0;
  int total_calls() { return ++calls; }
  int add(int a, int b) { return a + b; }
};

CURRENT_OBJECT_INTERFACE(NamespacedObjectWithInterface) {
  CURRENT_FORWARD_METHOD(total_calls);
  CURRENT_FORWARD_METHOD(add);
};

template <typename T>
struct NamespacedObjectContainer {
  T object;
  template <typename... ARGS>
  NamespacedObjectContainer(ARGS&&... args)
      : object(std::forward<ARGS>(args)...) {}
};

template <typename T>
struct NamespacedExampleObjectInterface : private NamespacedObjectContainer<T>,
                                          public CURRENT_OBJECT_INTERFACE_METHODS_WRAPPER<T> {
  using object_container_t = NamespacedObjectContainer<T>;
  using methods_wrapper_t = CURRENT_OBJECT_INTERFACE_METHODS_WRAPPER<T>;

  template <typename... ARGS>
  NamespacedExampleObjectInterface(ARGS&&... args)
      : object_container_t(std::forward<ARGS>(args)...) {
    methods_wrapper_t::CURRENT_IMPLEMENTATION_POINTER = &(object_container_t::object);
  }
};

}  // namespace object_with_interface_test_namespace

TEST(ObjectInterface, Smoke) {
  {
    ObjectWithInterface o;
    EXPECT_EQ(1, o.total_calls());
    EXPECT_EQ(2, o.total_calls());
    EXPECT_EQ(3, o.total_calls());
    EXPECT_EQ(1001, o.add(1000, 1));
  }
  {
    ExampleObjectInterface<ObjectWithInterface> o;
    EXPECT_EQ(1, o.total_calls());
    EXPECT_EQ(2, o.total_calls());
    EXPECT_EQ(3, o.total_calls());
    EXPECT_EQ(1001, o.add(1000, 1));
  }
  {
    object_with_interface_test_namespace::NamespacedObjectWithInterface o;
    EXPECT_EQ(1, o.total_calls());
    EXPECT_EQ(2, o.total_calls());
    EXPECT_EQ(3, o.total_calls());
    EXPECT_EQ(1001, o.add(1000, 1));
  }
  {
    object_with_interface_test_namespace::NamespacedExampleObjectInterface<
        object_with_interface_test_namespace::NamespacedObjectWithInterface> o;
    EXPECT_EQ(1, o.total_calls());
    EXPECT_EQ(2, o.total_calls());
    EXPECT_EQ(3, o.total_calls());
    EXPECT_EQ(1001, o.add(1000, 1));
  }
}
