/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#include "../../3rdparty/gtest/gtest-main.h"

// Basically, disable this test on clang++@Linux@Travis, because header f*ckup. -- D.K.
#if defined(HAS_CODECVT_HEADER) && !(defined(__clang__) && defined(CURRENT_LINUX) && defined(CURRENT_CI))

#include "ot.h"
#include "../../Bricks/file/file.h"

TEST(OperationalTransformation, Golden) {
  EXPECT_EQ(current::FileSystem::ReadFileAsString(current::FileSystem::JoinPath("golden", "data.txt")),
            current::utils::ot::OT(
                current::FileSystem::ReadFileAsString(current::FileSystem::JoinPath("golden", "data.ot"))));
}

#else

TEST(OperationalTransformation, DISABLED_GoldenTestDueToNoHeaderInLibStdCPlusPlus) {
  // Will print a yellow "You have 1 disabled test." to the console. That's all we need.
}

#endif  // defined(HAS_CODECVT_HEADER) && !(defined(__clang__) && defined(CURRENT_LINUX) && defined(CURRENT_CI))
