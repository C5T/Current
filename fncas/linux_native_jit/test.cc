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
TEST(LinuxNativeJIT, CopyPastedOpcodesCanBeExecuted) {
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
    0xf2, 0x0f, 0x10, 0x07, // movsd (%rdi), %xmm0
    0xf2, 0x0f, 0x58, 0x07, // addsd (%rdi), %xmm0
    0xc3                    // ret
  });
  // clang-format on

  current::fncas::linux_native_jit::CallableVectorUInt8 f(code);

  double x[1] = {1.0};
  EXPECT_EQ(2.0, f(x + 16, nullptr, nullptr));

  x[0] = 42;
  EXPECT_EQ(84.0, f(x + 16, nullptr, nullptr));

  x[0] = 1.5;
  EXPECT_EQ(3.0, f(x + 16, nullptr, nullptr));
}

TEST(LinuxNativeJIT, LoadsImmediateValues) {
  using namespace current::fncas::linux_native_jit::opcodes;

  double const d1 = 3.14;
  double const d2 = 2.17;
  double const d3 = 0.1234;
  double const d4 = 9.8765;

  std::vector<uint8_t> code;

  load_immediate_to_memory_by_rdi_offset(code, 0, d1);
  load_immediate_to_memory_by_rdi_offset(code, 1, d2);
  load_immediate_to_memory_by_rsi_offset(code, 2, d3);
  load_immediate_to_memory_by_rsi_offset(code, 3, d4);
  load_from_memory_by_offset_to_xmm0(code, r::rdi, 0);

  ret(code);

  std::vector<double> x(1000, 1.0);
  std::vector<double> y(1000, 2.0);

  EXPECT_EQ(1.0, x[0]);
  EXPECT_EQ(1.0, x[1]);
  EXPECT_EQ(1.0, x[2]);
  EXPECT_EQ(1.0, x[3]);
  EXPECT_EQ(2.0, y[0]);
  EXPECT_EQ(2.0, y[1]);
  EXPECT_EQ(2.0, y[2]);
  EXPECT_EQ(2.0, y[3]);

  EXPECT_EQ(d1, current::fncas::linux_native_jit::CallableVectorUInt8(code)(&x[0], &y[0], nullptr));

  EXPECT_NE(1.0, x[0]);
  EXPECT_NE(1.0, x[1]);
  EXPECT_EQ(1.0, x[2]);
  EXPECT_EQ(1.0, x[3]);
  EXPECT_EQ(2.0, y[0]);
  EXPECT_EQ(2.0, y[1]);
  EXPECT_NE(2.0, y[2]);
  EXPECT_NE(2.0, y[3]);
  EXPECT_EQ(d1, x[0]);
  EXPECT_EQ(d2, x[1]);
  EXPECT_EQ(d3, y[2]);
  EXPECT_EQ(d4, y[3]);
}

TEST(LinuxNativeJIT, LoadsRegistersFromMemory) {
  using namespace current::fncas::linux_native_jit::opcodes;

  std::vector<double> x(1000);
  for (size_t i = 0; i < x.size(); ++i) {
    x[i] = i;
  }

  std::vector<double> y(1000);
  for (size_t i = 0; i < y.size(); ++i) {
    y[i] = 1000 - i;
  }

  {
    // Return X[0].
    std::vector<uint8_t> code;
    load_from_memory_by_offset_to_xmm0(code, r::rdi, 0);
    ret(code);
    EXPECT_EQ(0, current::fncas::linux_native_jit::CallableVectorUInt8(code)(&x[0], &y[0], nullptr));
  }

  {
    // Return X[259].
    std::vector<uint8_t> code;
    load_from_memory_by_offset_to_xmm0(code, r::rdi, 259);
    ret(code);
    EXPECT_EQ(259, current::fncas::linux_native_jit::CallableVectorUInt8(code)(&x[0], &y[0], nullptr));
  }

  {
    // Return Y[0].
    std::vector<uint8_t> code;
    load_from_memory_by_offset_to_xmm0(code, r::rsi, 0);
    ret(code);
    EXPECT_EQ(1000, current::fncas::linux_native_jit::CallableVectorUInt8(code)(&x[0], &y[0], nullptr));
  }

  {
    // Return Y[931].
    std::vector<uint8_t> code;
    load_from_memory_by_offset_to_xmm0(code, r::rsi, 931);
    ret(code);
    EXPECT_EQ(1000 - 931, current::fncas::linux_native_jit::CallableVectorUInt8(code)(&x[0], &y[0], nullptr));
  }
}

TEST(LinuxNativeJIT, Adds) {
  using namespace current::fncas::linux_native_jit::opcodes;

  std::vector<uint8_t> code;

  load_immediate_to_memory_by_rdi_offset(code, 0, 11);
  load_immediate_to_memory_by_rdi_offset(code, 1, 12);
  load_immediate_to_memory_by_rdi_offset(code, 2, 13);
  load_immediate_to_memory_by_rsi_offset(code, 0, 14);
  load_immediate_to_memory_by_rsi_offset(code, 1, 15);
  load_immediate_to_memory_by_rsi_offset(code, 2, 16);

  // Let y[0] = x[1] + x[2].
  load_from_memory_by_offset_to_xmm0(code, r::rdi, 1);
  add_from_memory_by_offset_to_xmm0(code, r::rdi, 2);
  store_xmm0_to_memory_by_offset(code, 0);
  // Let y[1] = y[0] + x[0]
  load_from_memory_by_offset_to_xmm0(code, r::rsi, 0);
  add_from_memory_by_offset_to_xmm0(code, r::rdi, 0);
  store_xmm0_to_memory_by_offset(code, 1);
  ret(code);

  std::vector<double> x(3, 1.0);
  std::vector<double> y(3, 2.0);

  EXPECT_EQ(1, x[0]);
  EXPECT_EQ(1, x[1]);
  EXPECT_EQ(1, x[2]);
  EXPECT_EQ(2, y[0]);
  EXPECT_EQ(2, y[1]);
  EXPECT_EQ(2, y[2]);

  EXPECT_EQ(36, current::fncas::linux_native_jit::CallableVectorUInt8(code)(&x[0], &y[0], nullptr));

  EXPECT_EQ(11, x[0]);
  EXPECT_EQ(12, x[1]);
  EXPECT_EQ(13, x[2]);
  EXPECT_EQ(25, y[0]);  // Originally `x[1]` plus `x[2]`, which is 12 + 13.
  EXPECT_EQ(36, y[1]);  // The `y[0]`, which is 25, plus `x[0]`, which is 11.
  EXPECT_EQ(16, y[2]);
}

TEST(LinuxNativeJIT, PerformsArithmetic) {
  using namespace current::fncas::linux_native_jit::opcodes;

  std::vector<uint8_t> code;

  load_from_memory_by_offset_to_xmm0(code, r::rdi, 0);
  add_from_memory_by_offset_to_xmm0(code, r::rsi, 0);
  store_xmm0_to_memory_by_offset(code, 0);

  load_from_memory_by_offset_to_xmm0(code, r::rdi, 1);
  sub_from_memory_by_offset_to_xmm0(code, r::rsi, 1);
  store_xmm0_to_memory_by_offset(code, 1);

  load_from_memory_by_offset_to_xmm0(code, r::rdi, 2);
  mul_from_memory_by_offset_to_xmm0(code, r::rsi, 2);
  store_xmm0_to_memory_by_offset(code, 2);

  load_from_memory_by_offset_to_xmm0(code, r::rdi, 3);
  div_from_memory_by_offset_to_xmm0(code, r::rsi, 3);
  store_xmm0_to_memory_by_offset(code, 3);

  ret(code);

  std::vector<double> x(4, 10.0);
  std::vector<double> y(4, 5.0);

  EXPECT_EQ(10.0, x[0]);
  EXPECT_EQ(10.0, x[1]);
  EXPECT_EQ(10.0, x[2]);
  EXPECT_EQ(10.0, x[3]);

  EXPECT_EQ(5.0, y[0]);
  EXPECT_EQ(5.0, y[1]);
  EXPECT_EQ(5.0, y[2]);
  EXPECT_EQ(5.0, y[3]);

  (current::fncas::linux_native_jit::CallableVectorUInt8(code))(&x[0], &y[0], nullptr);

  EXPECT_EQ(15.0, y[0]);
  EXPECT_EQ(5.0, y[1]);
  EXPECT_EQ(50.0, y[2]);
  EXPECT_EQ(2.0, y[3]);
}

TEST(LinuxNativeJIT, CallsExternalFunctions) {
  using namespace current::fncas::linux_native_jit::opcodes;

  double (*f[])(double x) = {sin, cos, exp};

  std::vector<uint8_t> code;

  push_rbx(code);

  // mov_rsi_rbx(code);

  load_from_memory_by_offset_to_xmm0(code, r::rdi, 0);
  push_rdi(code);
  push_rsi(code);
  push_rdx(code);
  call_function(code, 0);
  pop_rdx(code);
  pop_rsi(code);
  pop_rdi(code);
  store_xmm0_to_memory_by_offset(code, 0);

  load_from_memory_by_offset_to_xmm0(code, r::rdi, 1);
  push_rsi(code);
  push_rdi(code);
  push_rsi(code);
  push_rdx(code);
  call_function(code, 1);
  pop_rdx(code);
  pop_rsi(code);
  pop_rdi(code);
  pop_rsi(code);
  store_xmm0_to_memory_by_offset(code, 1);

  load_from_memory_by_offset_to_xmm0(code, r::rdi, 2);
  push_rsi(code);
  push_rsi(code);
  push_rdi(code);
  push_rsi(code);
  push_rdx(code);
  call_function(code, 2);
  pop_rdx(code);
  pop_rsi(code);
  pop_rdi(code);
  pop_rsi(code);
  pop_rsi(code);
  store_xmm0_to_memory_by_offset(code, 2);

  pop_rbx(code);

  ret(code);

  std::vector<double> x(3, 1.0);
  std::vector<double> y(3);

  (current::fncas::linux_native_jit::CallableVectorUInt8(code))(&x[0], &y[0], f);

  EXPECT_EQ(sin(1.0), y[0]);
  EXPECT_EQ(cos(1.0), y[1]);
  EXPECT_EQ(exp(1.0), y[2]);
}

#endif  // FNCAS_LINUX_NATIVE_JIT_ENABLED
