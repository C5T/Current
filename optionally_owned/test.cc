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

#include "optionally_owned.h"

#include <string>
#include <thread>
#include <atomic>

#include "../../Bricks/3party/gtest/gtest-main.h"

using std::string;
using std::thread;
using std::this_thread::sleep_for;
using std::chrono::milliseconds;
using std::atomic_bool;

using sherlock::OptionallyOwned;

struct Foo {
  string& s_;
  Foo(string& s) : s_(s) { s_ += "+"; }
  ~Foo() { s_ += "-"; }
  Foo(Foo&&) = default;
  void Use() { s_ += "U"; }
};

TEST(OptionallyOwned, DoesNotOwnByReference) {
  string s;
  ASSERT_EQ("", s);
  {
    Foo bar(s);
    ASSERT_EQ("+", s);
    {
      OptionallyOwned<Foo> owner(bar);
      ASSERT_TRUE(owner.IsValid());
      ASSERT_FALSE(owner.IsDetachable());
      ASSERT_EQ("+", owner->s_);
    }
    ASSERT_EQ("+", s);  // The destructor of `bar` will only be called when its gets out of scope.
  }
  ASSERT_EQ("+-", s);
}

TEST(OptionallyOwned, DoesOwnByUniquePointer) {
  string s;
  ASSERT_EQ("", s);
  {
    std::unique_ptr<Foo> baz(new Foo(s));
    ASSERT_EQ("+", s);
    {
      OptionallyOwned<Foo> owner(std::move(baz));
      ASSERT_TRUE(owner.IsValid());
      ASSERT_TRUE(owner.IsDetachable());
      ASSERT_EQ("+", owner->s_);
    }
    ASSERT_EQ("+-", s);  // Since `owner` owned `baz`, it has been destructed as `owner` left the scope.
  }
  ASSERT_EQ("+-", s);
}

TEST(OptionallyOwned, ByUniquePointerOwnershipTransfers) {
  string s;
  ASSERT_EQ("", s);
  {
    std::unique_ptr<Foo> meh(new Foo(s));
    ASSERT_EQ("+", s);
    {
      OptionallyOwned<Foo> owner(std::move(meh));
      ASSERT_TRUE(owner.IsValid());
      ASSERT_TRUE(owner.IsDetachable());
      ASSERT_EQ("+", owner->s_);
      {
        OptionallyOwned<Foo> new_owner(std::move(owner));
        ASSERT_TRUE(new_owner.IsValid());
        ASSERT_EQ("+", new_owner->s_);
        ASSERT_FALSE(owner.IsValid());
        string tmp;
        ASSERT_THROW(tmp = owner->s_, std::logic_error);
      }
      // Since `owner` has transferred ownership of `meh` to `new_owner`, which went out of scope,
      // `meh` has been destroyed as the scope ending on the previous line has exited.
      ASSERT_EQ("+-", s);
    }
    ASSERT_EQ("+-", s);  // Since `owner` owned `meh`, it has been destructed as `owner` left the scope.
  }
  ASSERT_EQ("+-", s);
}

inline void UseFoo(OptionallyOwned<Foo> foo) { foo->Use(); }

TEST(OptionallyOwned, ByUniquePointerOwnershipCanBePassedOnToAnotherFunction) {
  string s;
  ASSERT_EQ("", s);
  {
    std::unique_ptr<Foo> woo(new Foo(s));
    ASSERT_EQ("+", s);
    {
      OptionallyOwned<Foo> owner(std::move(woo));
      ASSERT_TRUE(owner.IsValid());
      ASSERT_TRUE(owner.IsDetachable());
      ASSERT_EQ("+", owner->s_);
      UseFoo(std::move(owner));
      ASSERT_FALSE(owner.IsValid());
      string tmp;
      ASSERT_THROW(tmp = owner->s_, std::logic_error);
      ASSERT_EQ("+U-", s);
    }
    ASSERT_EQ("+U-", s);
  }
  ASSERT_EQ("+U-", s);
}

TEST(OptionallyOwned, ByUniquePointerOwnershipCanBePassedOnToAnotherThread) {
  string s;
  ASSERT_EQ("", s);
  atomic_bool kill(false);
  atomic_bool done(false);
  std::unique_ptr<Foo> whoa(new Foo(s));
  ASSERT_EQ("+", s);
  {
    OptionallyOwned<Foo> owner(std::move(whoa));
    ASSERT_TRUE(owner.IsValid());
    ASSERT_TRUE(owner.IsDetachable());
    thread([&kill, &done](OptionallyOwned<Foo> inner_owned) {
             sleep_for(milliseconds(1));
             while (!kill) {
               ;  // Spin lock;
             }
             sleep_for(milliseconds(1));
             { OptionallyOwned<Foo> inner_owned_and_let_go(std::move(inner_owned)); }
             sleep_for(milliseconds(1));
             done = true;
           },
           std::move(owner)).detach();
    ASSERT_FALSE(owner.IsValid());
    string tmp;
    ASSERT_THROW(tmp = owner->s_, std::logic_error);
    ASSERT_EQ("+", s);
  }
  sleep_for(milliseconds(1));
  ASSERT_EQ("+", s);  // An instance of `whoa` should still be alive in the thread.
  kill = true;
  while (!done) {
    ;  // Spin lock;
  }
  ASSERT_EQ("+-", s);  // An instance of `whoa` is now gone since the thread has released it.
}
