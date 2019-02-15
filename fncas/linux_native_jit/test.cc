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

#include "../../3rdparty/gtest/gtest-main.h"

#include "linux_native_jit.h"

#ifdef FNCAS_LINUX_NATIVE_JIT_ENABLED

// Tests the `mmap()` and `mprotect()` combination, as well as the calling convention.
TEST(LinuxNativeJIT, LowestLevelTest) {
#if 0
Steps to re-create the hexadecimal opcodes for this simple test:

$ cat 1.cc 
   extern "C" double test(double const* x, double*, double (*[])(double x)) { return x[0] + x[0]; }

$ g++ -c -O3 1.cc && objdump -S 1.o
   ...
   0:	f2 0f 10 07          	movsd  (%rdi),%xmm0
   4:	f2 0f 58 c0          	addsd  %xmm0,%xmm0
   8:	c3                   	retq

#endif

  // clang-format off
  std::vector<uint8_t> code({
    0x53,                   // push  %rbx
    0xf2, 0x0f, 0x10, 0x07, // movsd (%rdi), %xmm0
    0xf2, 0x0f, 0x58, 0x07, // addsd (%rdi), %xmm0
    0x5b,                   // pop   %rbx
    0xc3                    // ret
  });
  // clang-format on 

  current::fncas::linux_native_jit::CallableVectorUInt8 f(code);

  double x[1] = { 1.0 };
  EXPECT_EQ(2.0, f(x, nullptr, nullptr));

  x[0] = 42;
  EXPECT_EQ(84.0, f(x, nullptr, nullptr));

  x[0] = 1.5;
  EXPECT_EQ(3.0, f(x, nullptr, nullptr));
}

#endif  // FNCAS_LINUX_NATIVE_JIT_ENABLED
