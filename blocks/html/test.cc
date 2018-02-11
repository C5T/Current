/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2018 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#define CURRENT_HTML_UNIT_TEST

#include "html.h"

#include "../../3rdparty/gtest/gtest-main.h"

// Tests sensitive to line numbers go first. Scroll down for functionality tests.
// clang-format off

TEST(HTMLTest, HTMLShouldCallBegin) {
  HTMLGenerator.ResetForUnitTest();
  EXPECT_EQ("Attempted to call HTMLGenerator.End() on an uninitialized HTMLGenerator @ UNITTEST:37\n",
            HTMLGenerator.End());
}

TEST(HTMLTest, HTMLShouldNotCallBeginTwice) {
  HTMLGenerator.ResetForUnitTest();
  HTMLGenerator.Begin();
  HTMLGenerator.Begin();
  EXPECT_EQ(
      "Attempted to call HTMLGenerator.Begin() more than once in a row @ UNITTEST:43\n"
      "Attempted to call HTMLGenerator.End() with critical errors @ UNITTEST:47\n",
      HTMLGenerator.End());
}

TEST(HTMLTest, HTMLShouldNotCallEndTwice) {
  HTMLGenerator.ResetForUnitTest();
  HTMLGenerator.Begin();
  EXPECT_EQ("", HTMLGenerator.End());
  EXPECT_EQ("Attempted to call HTMLGenerator.End() more than once in a row @ UNITTEST:54\n", HTMLGenerator.End());
}

// clang-format on
// Now, tests the functionality.

TEST(HTMLTest, Trivial) {
  HTMLGenerator.Begin();
  HTML(UnsafeText) << "Hello, World!";
  EXPECT_EQ("Hello, World!", HTMLGenerator.End());

  HTMLGenerator.Begin();
  HTML(UnsafeText) << "And once again.";
  EXPECT_EQ("And once again.", HTMLGenerator.End());
}

TEST(HTMLTest, Smoke) {
  // HTML.
  // HEAD.
  // B, I.
  // A, with a parameter.
  // FONT, with a parameter.
  // TABLE, TR, TD, with parameters.
  // Nested.
}

TEST(HTMLTest, ThreadIsolation) {}

TEST(HtmlTest, HTTPIntegration) {
  // TODO(dkorolev): This.
}
