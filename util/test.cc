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

#include "make_scope_guard.h"
#include "singleton.h"

#include "../3party/gtest/gtest-main.h"

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
    size_t bar = 0;
    void baz() {
      ++bar;
    }
  };
  EXPECT_EQ(0u, bricks::Singleton<Foo>().bar);
  bricks::Singleton<Foo>().baz();
  EXPECT_EQ(1u, bricks::Singleton<Foo>().bar);
  const auto lambda = []() {
    bricks::Singleton<Foo>().baz();
  };
  EXPECT_EQ(1u, bricks::Singleton<Foo>().bar);
  lambda();
  EXPECT_EQ(2u, bricks::Singleton<Foo>().bar);
}
