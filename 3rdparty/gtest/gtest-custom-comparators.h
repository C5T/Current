/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2019 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef THIRDPARTY_GTEST_CUSTOM_COMPARATORS_H
#define THIRDPARTY_GTEST_CUSTOM_COMPARATORS_H

#include <string>
#include <ostream>
#include <functional>  // For `std::decay_t<>`, reluctant to bring in `current::decay` here.

// NOTE(dkorolev): This file is added manually to allow writing cleaner unit tests for external user-defined types.
//                 The user can partially specialize `CustomComparator` for their own types in `namespace ::gtest`,
//                 and use `EXPECT_EQ` / `EXPECT_NE` in their unit tests to do the job natively.
namespace gtest {
template <typename LHS, typename RHS>
struct CustomComparator {
  template <typename LHS_INSTANCE, typename RHS_INSTANCE>
  static bool EXPECT_EQ_IMPL(LHS_INSTANCE&& lhs, RHS_INSTANCE&& rhs) {
    return std::forward<LHS_INSTANCE>(lhs) == std::forward<RHS_INSTANCE>(rhs);
  }
  template <typename LHS_INSTANCE, typename RHS_INSTANCE>
  static bool EXPECT_NE_IMPL(LHS_INSTANCE&& lhs, RHS_INSTANCE&& rhs) {
    return std::forward<LHS_INSTANCE>(lhs) != std::forward<RHS_INSTANCE>(rhs);
  }
};
}  // namespace gtest

#endif  // THIRDPARTY_GTEST_CUSTOM_COMPARATORS_H
