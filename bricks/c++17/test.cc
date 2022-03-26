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

#include "../../typesystem/reflection/reflection.h"

#include "../../3rdparty/gtest/gtest-main.h"

#if __cplusplus >= 201703L
TEST(CPlusPlus17, Enabled) {
  auto const generic_lambda = [](auto const x) {
    return current::reflection::TypeName<current::decay_t<decltype(x)>>();
  };

  EXPECT_STREQ("uint32_t", generic_lambda(static_cast<uint32_t>(42)));
  EXPECT_STREQ("bool", generic_lambda(true));
  EXPECT_STREQ("std::string", generic_lambda(std::string("foo")));
  EXPECT_STREQ("std::vector<uint32_t>", generic_lambda(std::vector<uint32_t>({1, 2, 3})));
}
#endif  // __cplusplus >= 201703L
