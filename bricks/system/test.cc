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

#define BRICKS_RANDOM_FIX_SEED

#include "syscalls.h"

#include "../../3rdparty/gtest/gtest-main.h"

#include <thread>

TEST(Syscalls, PipedOutputSingleLine) {
  EXPECT_EQ("Hello, World!", current::bricks::system::InputTextPipe("echo 'Hello, World!'").ReadLine());
}

TEST(Syscalls, PipedOutputMultipleLines) {
  current::bricks::system::InputTextPipe pipe("echo Hello; echo World");
  EXPECT_TRUE(pipe);
  EXPECT_EQ("Hello", pipe.ReadLine());
  EXPECT_TRUE(pipe);
  EXPECT_EQ("World", pipe.ReadLine());
  // Uncertain whether it should be EOF here or no, but it sure will be after the next, "empty", line is read.
  EXPECT_EQ("", pipe.ReadLine());
  EXPECT_FALSE(pipe);
}
