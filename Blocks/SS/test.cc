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

#include "ss.h"

#include <string>

#include "../../Bricks/strings/printf.h"
#include "../../3rdparty/gtest/gtest-main.h"

using current::strings::Printf;

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

TEST(StreamSystem, TestHelperClassSmokeTest) {
  using DDE = DispatchDemoEntry;

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
