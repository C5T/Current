/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_RTTI_DOCU_01
#define BRICKS_RTTI_DOCU_01

#include "../dispatcher.h"
#include "../../strings/printf.h"

#include "../../../3rdparty/gtest/gtest-main.h"

using current::strings::Printf;

#if 0
  // The example below uses `Printf()`, include it.
  #include "strings/printf.h"
  using current::strings::Printf;
   
#endif

  struct ExampleBase {
    virtual ~ExampleBase() = default;
  };
  
  struct ExampleInt : ExampleBase {
    int i;
    explicit ExampleInt(int i) : i(i) {}
  };
  
  struct ExampleString : ExampleBase {
    std::string s;
    explicit ExampleString(const std::string& s) : s(s) {}
  };
  
  struct ExampleMoo : ExampleBase {
  };
  
  struct ExampleProcessor {
    std::string result;
    void operator()(const ExampleBase&) { result = "unknown"; }
    void operator()(const ExampleInt& x) { result = Printf("int %d", x.i); }
    void operator()(const ExampleString& x) { result = Printf("string '%s'", x.s.c_str()); }
    void operator()(const ExampleMoo&) { result = "moo!"; }
  };
  

TEST(RTTIDocu, Docu01) {
  using current::rtti::RuntimeTypeListDispatcher;
  typedef RuntimeTypeListDispatcher<ExampleBase,
                                    TypeListImpl<ExampleInt, ExampleString, ExampleMoo>> Dispatcher;
  
  ExampleProcessor processor;
  
  Dispatcher::DispatchCall(ExampleBase(), processor);
  EXPECT_EQ(processor.result, "unknown");
  
  Dispatcher::DispatchCall(ExampleInt(42), processor);
  EXPECT_EQ(processor.result, "int 42");
  
  Dispatcher::DispatchCall(ExampleString("foo"), processor);
  EXPECT_EQ(processor.result, "string 'foo'");
  
  Dispatcher::DispatchCall(ExampleMoo(), processor);
  EXPECT_EQ(processor.result, "moo!");
}

#endif  // BRICKS_RTTI_DOCU_01
