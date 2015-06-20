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

#include "../../port.h"

#include "ss.h"

#include <string>

#include "../../Bricks/strings/printf.h"
#include "../../3rdparty/gtest/gtest-main.h"

using bricks::strings::Printf;

// A copy-able, move-able and clone-able entry.
struct DispatchDemoEntry {
  std::string text;
  DispatchDemoEntry() : text("Entry()") {}
  DispatchDemoEntry(const DispatchDemoEntry& rhs) : text("Entry(copy of {" + rhs.text + "})") {}
  DispatchDemoEntry(const std::string& text) : text("Entry('" + text + "')") {}
  DispatchDemoEntry(DispatchDemoEntry&& rhs) : text("Entry(move of {" + rhs.text + "})") {
    rhs.text = "moved away {" + rhs.text + "}";
  }

  struct Clone {};
  DispatchDemoEntry(Clone, const DispatchDemoEntry& rhs) : text("Entry(clone of {" + rhs.text + "})") {}

  DispatchDemoEntry& operator=(const DispatchDemoEntry&) = delete;
  DispatchDemoEntry& operator=(DispatchDemoEntry&&) = delete;
};

TEST(MQInterface, TestHelperClassSmokeTest) {
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

  DDE e5(DDE::Clone(), e1);
  EXPECT_EQ("Entry()", e1.text);
  EXPECT_EQ("Entry(clone of {Entry()})", e5.text);
}

// Demonstrates that the entry is only copied/cloned if the dispatcher needs a copy.
// This test also confirms the dispatcher supports lambdas.
// The rest of the functionality -- treating `void` as `bool` and enabling omitting extra parameters --
// is tested in a giant write-only test below.
TEST(MQInterface, SmokeTest) {
  using DDE = DispatchDemoEntry;

  const std::function<DDE(const DDE& e)> clone_f = [](const DDE& e) -> DDE { return DDE(DDE::Clone(), e); };

  std::string text;

  const auto does_not_need_a_copy_lambda = [&text](const DDE& e, size_t count, size_t total) {
    text = Printf("%s, %d, %d", e.text.c_str(), int(count), int(total));
    return false;
  };

  const auto need_a_copy_lambda = [&text](DDE&& e, size_t count, size_t total) {
    text = Printf("%s, %d, %d", e.text.c_str(), int(count), int(total));
    e.text = "invalidated <" + e.text + ">";
    return false;
  };

  // The two parameters after `entry` -- `index` and `total` -- do not carry any value for this test,
  // we just confirm they are passed through.
  DDE e;

  ASSERT_FALSE(blocks::ss::DispatchEntryByConstReference(does_not_need_a_copy_lambda, e, 1, 4));
  EXPECT_EQ("Entry(), 1, 4", text);

  ASSERT_FALSE(blocks::ss::DispatchEntryByConstReference(does_not_need_a_copy_lambda, e, 2, 3, clone_f));
  EXPECT_EQ("Entry(), 2, 3", text);

  ASSERT_FALSE(blocks::ss::DispatchEntryByConstReference(need_a_copy_lambda, e, 1, 2));
  EXPECT_EQ("Entry(copy of {Entry()}), 1, 2", text);

  ASSERT_FALSE(blocks::ss::DispatchEntryByConstReference(need_a_copy_lambda, e, 3, 4, clone_f));
  EXPECT_EQ("Entry(clone of {Entry()}), 3, 4", text);

  EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

  ASSERT_FALSE(blocks::ss::DispatchEntryByRValue(does_not_need_a_copy_lambda, e, 1, 3));
  EXPECT_EQ("Entry(), 1, 3", text);

  EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

  ASSERT_FALSE(blocks::ss::DispatchEntryByRValue(need_a_copy_lambda, std::move(e), 2, 4));
  EXPECT_EQ("Entry(), 2, 4", text);

  EXPECT_EQ("invalidated <Entry()>", e.text) << "The original `DDE()` should be invalidated.";
}

// This following test is write-only code. It's `SmokeTest` copied over multiple times with minor changes.
// Just make sure it passes. You really don't have to go through it in detail.
// Okay, if you have to -- sorry! -- it introduces changes to `SmokeTest` across two dimensions:
// 1) Return type: `bool` -> `void`.
// 2) Number of arguments: 3 -> 2 -> 1.
// You've been warned. Thanks!
TEST(MQInterface, WriteOnlyTestTheRemainingCasesOutOfThoseTwelve) {
  using DDE = DispatchDemoEntry;

  const std::function<DDE(const DDE& e)> clone_f = [](const DDE& e) -> DDE { return DDE(DDE::Clone(), e); };

  // The copy-paste of `SmokeTest`, as the boilerplate.
  {
    struct DoesNotNeedACopy {
      std::string text;
      bool operator()(const DDE& e, size_t count, size_t total) {
        text += Printf("%s, %d, %d", e.text.c_str(), int(count), int(total));
        return false;
      }
    };

    struct NeedsACopy {
      std::string text;
      bool operator()(DDE&& e, size_t count, size_t total) {
        text += Printf("%s, %d, %d", e.text.c_str(), int(count), int(total));
        e.text = "invalidated <" + e.text + ">";
        return false;
      }
    };

    DoesNotNeedACopy does_not_need_a_copy_1;
    DoesNotNeedACopy does_not_need_a_copy_2;
    DoesNotNeedACopy does_not_need_a_copy_3;

    NeedsACopy needs_a_copy_1;
    NeedsACopy needs_a_copy_2;
    NeedsACopy needs_a_copy_3;

    DDE e;

    ASSERT_FALSE(blocks::ss::DispatchEntryByConstReference(does_not_need_a_copy_1, e, 1, 4));
    EXPECT_EQ("Entry(), 1, 4", does_not_need_a_copy_1.text);

    ASSERT_FALSE(blocks::ss::DispatchEntryByConstReference(does_not_need_a_copy_2, e, 2, 3, clone_f));
    EXPECT_EQ("Entry(), 2, 3", does_not_need_a_copy_2.text);

    ASSERT_FALSE(blocks::ss::DispatchEntryByConstReference(needs_a_copy_1, e, 1, 2));
    EXPECT_EQ("Entry(copy of {Entry()}), 1, 2", needs_a_copy_1.text);

    ASSERT_FALSE(blocks::ss::DispatchEntryByConstReference(needs_a_copy_2, e, 3, 4, clone_f));
    EXPECT_EQ("Entry(clone of {Entry()}), 3, 4", needs_a_copy_2.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_FALSE(blocks::ss::DispatchEntryByRValue(does_not_need_a_copy_3, e, 1, 3));
    EXPECT_EQ("Entry(), 1, 3", does_not_need_a_copy_3.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_FALSE(blocks::ss::DispatchEntryByRValue(needs_a_copy_3, std::move(e), 2, 4));
    EXPECT_EQ("Entry(), 2, 4", needs_a_copy_3.text);

    EXPECT_EQ("invalidated <Entry()>", e.text) << "The original `DDE()` should be invalidated.";
  }

  // Confirm that `void` return type is treated as `bool true`.
  {
    struct DoesNotNeedACopy {
      std::string text;
      void operator()(const DDE& e, size_t count, size_t total) {
        text += Printf("%s, %d, %d", e.text.c_str(), int(count), int(total));
      }
    };

    struct NeedsACopy {
      std::string text;
      void operator()(DDE&& e, size_t count, size_t total) {
        text += Printf("%s, %d, %d", e.text.c_str(), int(count), int(total));
        e.text = "invalidated <" + e.text + ">";
      }
    };

    DoesNotNeedACopy does_not_need_a_copy_1;
    DoesNotNeedACopy does_not_need_a_copy_2;
    DoesNotNeedACopy does_not_need_a_copy_3;

    NeedsACopy needs_a_copy_1;
    NeedsACopy needs_a_copy_2;
    NeedsACopy needs_a_copy_3;

    DDE e;

    ASSERT_TRUE(blocks::ss::DispatchEntryByConstReference(does_not_need_a_copy_1, e, 1, 4));
    EXPECT_EQ("Entry(), 1, 4", does_not_need_a_copy_1.text);

    ASSERT_TRUE(blocks::ss::DispatchEntryByConstReference(does_not_need_a_copy_2, e, 2, 3, clone_f));
    EXPECT_EQ("Entry(), 2, 3", does_not_need_a_copy_2.text);

    ASSERT_TRUE(blocks::ss::DispatchEntryByConstReference(needs_a_copy_1, e, 1, 2));
    EXPECT_EQ("Entry(copy of {Entry()}), 1, 2", needs_a_copy_1.text);

    ASSERT_TRUE(blocks::ss::DispatchEntryByConstReference(needs_a_copy_2, e, 3, 4, clone_f));
    EXPECT_EQ("Entry(clone of {Entry()}), 3, 4", needs_a_copy_2.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_TRUE(blocks::ss::DispatchEntryByRValue(does_not_need_a_copy_3, e, 1, 3));
    EXPECT_EQ("Entry(), 1, 3", does_not_need_a_copy_3.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_TRUE(blocks::ss::DispatchEntryByRValue(needs_a_copy_3, std::move(e), 2, 4));
    EXPECT_EQ("Entry(), 2, 4", needs_a_copy_3.text);

    EXPECT_EQ("invalidated <Entry()>", e.text) << "The original `DDE()` should be invalidated.";
  }

  // The copy-paste of `SmokeTest`, with two, not three, parameters for `operator()`.
  {
    struct DoesNotNeedACopy {
      std::string text;
      bool operator()(const DDE& e, size_t count) {
        text += Printf("%s, %d", e.text.c_str(), int(count));
        return false;
      }
    };

    struct NeedsACopy {
      std::string text;
      bool operator()(DDE&& e, size_t count) {
        text += Printf("%s, %d", e.text.c_str(), int(count));
        e.text = "invalidated <" + e.text + ">";
        return false;
      }
    };

    DoesNotNeedACopy does_not_need_a_copy_1;
    DoesNotNeedACopy does_not_need_a_copy_2;
    DoesNotNeedACopy does_not_need_a_copy_3;

    NeedsACopy needs_a_copy_1;
    NeedsACopy needs_a_copy_2;
    NeedsACopy needs_a_copy_3;

    DDE e;

    ASSERT_FALSE(blocks::ss::DispatchEntryByConstReference(does_not_need_a_copy_1, e, 100, 0));
    EXPECT_EQ("Entry(), 100", does_not_need_a_copy_1.text);

    ASSERT_FALSE(blocks::ss::DispatchEntryByConstReference(does_not_need_a_copy_2, e, 101, 0, clone_f));
    EXPECT_EQ("Entry(), 101", does_not_need_a_copy_2.text);

    ASSERT_FALSE(blocks::ss::DispatchEntryByConstReference(needs_a_copy_1, e, 102, 0));
    EXPECT_EQ("Entry(copy of {Entry()}), 102", needs_a_copy_1.text);

    ASSERT_FALSE(blocks::ss::DispatchEntryByConstReference(needs_a_copy_2, e, 103, 0, clone_f));
    EXPECT_EQ("Entry(clone of {Entry()}), 103", needs_a_copy_2.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_FALSE(blocks::ss::DispatchEntryByRValue(does_not_need_a_copy_3, e, 104, 0));
    EXPECT_EQ("Entry(), 104", does_not_need_a_copy_3.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_FALSE(blocks::ss::DispatchEntryByRValue(needs_a_copy_3, std::move(e), 105, 0));
    EXPECT_EQ("Entry(), 105", needs_a_copy_3.text);

    EXPECT_EQ("invalidated <Entry()>", e.text) << "The original `DDE()` should be invalidated.";
  }

  // Confirm that `void` return type is treated as `bool true`, with two parameters for `operator()`
  {
    struct DoesNotNeedACopy {
      std::string text;
      void operator()(const DDE& e, size_t count) { text += Printf("%s, %d", e.text.c_str(), int(count)); }
    };

    struct NeedsACopy {
      std::string text;
      void operator()(DDE&& e, size_t count) {
        text += Printf("%s, %d", e.text.c_str(), int(count));
        e.text = "invalidated <" + e.text + ">";
      }
    };

    DoesNotNeedACopy does_not_need_a_copy_1;
    DoesNotNeedACopy does_not_need_a_copy_2;
    DoesNotNeedACopy does_not_need_a_copy_3;

    NeedsACopy needs_a_copy_1;
    NeedsACopy needs_a_copy_2;
    NeedsACopy needs_a_copy_3;

    DDE e;

    ASSERT_TRUE(blocks::ss::DispatchEntryByConstReference(does_not_need_a_copy_1, e, 200, 0));
    EXPECT_EQ("Entry(), 200", does_not_need_a_copy_1.text);

    ASSERT_TRUE(blocks::ss::DispatchEntryByConstReference(does_not_need_a_copy_2, e, 201, 0, clone_f));
    EXPECT_EQ("Entry(), 201", does_not_need_a_copy_2.text);

    ASSERT_TRUE(blocks::ss::DispatchEntryByConstReference(needs_a_copy_1, e, 202, 0));
    EXPECT_EQ("Entry(copy of {Entry()}), 202", needs_a_copy_1.text);

    ASSERT_TRUE(blocks::ss::DispatchEntryByConstReference(needs_a_copy_2, e, 203, 0, clone_f));
    EXPECT_EQ("Entry(clone of {Entry()}), 203", needs_a_copy_2.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_TRUE(blocks::ss::DispatchEntryByRValue(does_not_need_a_copy_3, e, 204, 0));
    EXPECT_EQ("Entry(), 204", does_not_need_a_copy_3.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_TRUE(blocks::ss::DispatchEntryByRValue(needs_a_copy_3, std::move(e), 205, 0));
    EXPECT_EQ("Entry(), 205", needs_a_copy_3.text);

    EXPECT_EQ("invalidated <Entry()>", e.text) << "The original `DDE()` should be invalidated.";
  }

  // The copy-paste of `SmokeTest`, with one, not three, parameters for `operator()`.
  {
    struct DoesNotNeedACopy {
      std::string text;
      bool operator()(const DDE& e) {
        text += e.text;
        return false;
      }
    };

    struct NeedsACopy {
      std::string text;
      bool operator()(DDE&& e) {
        text += e.text;
        e.text = "invalidated <" + e.text + ">";
        return false;
      }
    };

    DoesNotNeedACopy does_not_need_a_copy_1;
    DoesNotNeedACopy does_not_need_a_copy_2;
    DoesNotNeedACopy does_not_need_a_copy_3;

    NeedsACopy needs_a_copy_1;
    NeedsACopy needs_a_copy_2;
    NeedsACopy needs_a_copy_3;

    DDE e;

    ASSERT_FALSE(blocks::ss::DispatchEntryByConstReference(does_not_need_a_copy_1, e, 300, 0));
    EXPECT_EQ("Entry()", does_not_need_a_copy_1.text);

    ASSERT_FALSE(blocks::ss::DispatchEntryByConstReference(does_not_need_a_copy_2, e, 301, 0, clone_f));
    EXPECT_EQ("Entry()", does_not_need_a_copy_2.text);

    ASSERT_FALSE(blocks::ss::DispatchEntryByConstReference(needs_a_copy_1, e, 302, 0));
    EXPECT_EQ("Entry(copy of {Entry()})", needs_a_copy_1.text);

    ASSERT_FALSE(blocks::ss::DispatchEntryByConstReference(needs_a_copy_2, e, 303, 0, clone_f));
    EXPECT_EQ("Entry(clone of {Entry()})", needs_a_copy_2.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_FALSE(blocks::ss::DispatchEntryByRValue(does_not_need_a_copy_3, e, 304, 0));
    EXPECT_EQ("Entry()", does_not_need_a_copy_3.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_FALSE(blocks::ss::DispatchEntryByRValue(needs_a_copy_3, std::move(e), 305, 0));
    EXPECT_EQ("Entry()", needs_a_copy_3.text);

    EXPECT_EQ("invalidated <Entry()>", e.text) << "The original `DDE()` should be invalidated.";
  }

  // Confirm that `void` return type is treated as `bool true`, with one parameter for `operator()`
  {
    struct DoesNotNeedACopy {
      std::string text;
      void operator()(const DDE& e) { text += e.text; }
    };

    struct NeedsACopy {
      std::string text;
      void operator()(DDE&& e) {
        text += e.text;
        e.text = "invalidated <" + e.text + ">";
      }
    };

    DoesNotNeedACopy does_not_need_a_copy_1;
    DoesNotNeedACopy does_not_need_a_copy_2;
    DoesNotNeedACopy does_not_need_a_copy_3;

    NeedsACopy needs_a_copy_1;
    NeedsACopy needs_a_copy_2;
    NeedsACopy needs_a_copy_3;

    DDE e;

    ASSERT_TRUE(blocks::ss::DispatchEntryByConstReference(does_not_need_a_copy_1, e, 400, 0));
    EXPECT_EQ("Entry()", does_not_need_a_copy_1.text);

    ASSERT_TRUE(blocks::ss::DispatchEntryByConstReference(does_not_need_a_copy_2, e, 401, 0, clone_f));
    EXPECT_EQ("Entry()", does_not_need_a_copy_2.text);

    ASSERT_TRUE(blocks::ss::DispatchEntryByConstReference(needs_a_copy_1, e, 402, 0));
    EXPECT_EQ("Entry(copy of {Entry()})", needs_a_copy_1.text);

    ASSERT_TRUE(blocks::ss::DispatchEntryByConstReference(needs_a_copy_2, e, 403, 0, clone_f));
    EXPECT_EQ("Entry(clone of {Entry()})", needs_a_copy_2.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_TRUE(blocks::ss::DispatchEntryByRValue(does_not_need_a_copy_3, e, 404, 0));
    EXPECT_EQ("Entry()", does_not_need_a_copy_3.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_TRUE(blocks::ss::DispatchEntryByRValue(needs_a_copy_3, std::move(e), 405, 0));
    EXPECT_EQ("Entry()", needs_a_copy_3.text);

    EXPECT_EQ("invalidated <Entry()>", e.text) << "The original `DDE()` should be invalidated.";
  }
}
