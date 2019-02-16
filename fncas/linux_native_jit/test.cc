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
  using namespace current::fncas::linux_native_jit;

  double const d1 = 3.14;
  double const d2 = 2.17;
  double const d3 = 0.1234;
  double const d4 = 9.8765;

  std::vector<uint8_t> code;

  opcodes::unsafe_load_immediate_to_memory_by_rdi_offset(code, 0, d1);
  opcodes::unsafe_load_immediate_to_memory_by_rdi_offset(code, 1, d2);
  opcodes::load_immediate_to_memory_by_rsi_offset(code, 2, d3);
  opcodes::load_immediate_to_memory_by_rsi_offset(code, 3, d4);
  opcodes::load_from_memory_by_rdi_offset_to_xmm0(code, 0);

  opcodes::ret(code);

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
  using namespace current::fncas::linux_native_jit;

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
    opcodes::load_from_memory_by_rdi_offset_to_xmm0(code, 0);
    opcodes::ret(code);
    EXPECT_EQ(0, current::fncas::linux_native_jit::CallableVectorUInt8(code)(&x[0], &y[0], nullptr));
  }

  {
    // Return X[259].
    std::vector<uint8_t> code;
    opcodes::load_from_memory_by_rdi_offset_to_xmm0(code, 259);
    opcodes::ret(code);
    EXPECT_EQ(259, current::fncas::linux_native_jit::CallableVectorUInt8(code)(&x[0], &y[0], nullptr));
  }

  {
    // Return Y[0].
    std::vector<uint8_t> code;
    opcodes::load_from_memory_by_rsi_offset_to_xmm0(code, 0);
    opcodes::ret(code);
    EXPECT_EQ(1000, current::fncas::linux_native_jit::CallableVectorUInt8(code)(&x[0], &y[0], nullptr));
  }

  {
    // Return Y[931].
    std::vector<uint8_t> code;
    opcodes::load_from_memory_by_rsi_offset_to_xmm0(code, 931);
    opcodes::ret(code);
    EXPECT_EQ(1000 - 931, current::fncas::linux_native_jit::CallableVectorUInt8(code)(&x[0], &y[0], nullptr));
  }
}

TEST(LinuxNativeJIT, Adds) {
  using namespace current::fncas::linux_native_jit;

  std::vector<uint8_t> code;

  opcodes::unsafe_load_immediate_to_memory_by_rdi_offset(code, 0, 11);
  opcodes::unsafe_load_immediate_to_memory_by_rdi_offset(code, 1, 12);
  opcodes::unsafe_load_immediate_to_memory_by_rdi_offset(code, 2, 13);
  opcodes::load_immediate_to_memory_by_rsi_offset(code, 0, 14);
  opcodes::load_immediate_to_memory_by_rsi_offset(code, 1, 15);
  opcodes::load_immediate_to_memory_by_rsi_offset(code, 2, 16);

  // Let y[0] = x[1] + x[2].
  opcodes::load_from_memory_by_rdi_offset_to_xmm0(code, 1);
  opcodes::add_from_memory_by_rdi_offset_to_xmm0(code, 2);
  opcodes::store_xmm0_to_memory_by_rsi_offset(code, 0);
  // Let y[1] = y[0] + x[0]
  opcodes::load_from_memory_by_rsi_offset_to_xmm0(code, 0);
  opcodes::add_from_memory_by_rdi_offset_to_xmm0(code, 0);
  opcodes::store_xmm0_to_memory_by_rsi_offset(code, 1);
  opcodes::ret(code);

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
  using namespace current::fncas::linux_native_jit;

  std::vector<uint8_t> code;

  opcodes::load_from_memory_by_rdi_offset_to_xmm0(code, 0);
  opcodes::add_from_memory_by_rsi_offset_to_xmm0(code, 0);
  opcodes::store_xmm0_to_memory_by_rsi_offset(code, 0);

  opcodes::load_from_memory_by_rdi_offset_to_xmm0(code, 1);
  opcodes::sub_from_memory_by_rsi_offset_to_xmm0(code, 1);
  opcodes::store_xmm0_to_memory_by_rsi_offset(code, 1);

  opcodes::load_from_memory_by_rdi_offset_to_xmm0(code, 2);
  opcodes::mul_from_memory_by_rsi_offset_to_xmm0(code, 2);
  opcodes::store_xmm0_to_memory_by_rsi_offset(code, 2);

  opcodes::load_from_memory_by_rdi_offset_to_xmm0(code, 3);
  opcodes::div_from_memory_by_rsi_offset_to_xmm0(code, 3);
  opcodes::store_xmm0_to_memory_by_rsi_offset(code, 3);

  opcodes::ret(code);

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

TEST(LinuxNativeJIT, FullySupportsRbx) {
  using namespace current::fncas::linux_native_jit;

  std::vector<uint8_t> code;

  opcodes::push_rbx(code);
  opcodes::mov_rsi_rbx(code);

  opcodes::load_from_memory_by_rdi_offset_to_xmm0(code, 0);
  opcodes::add_from_memory_by_rbx_offset_to_xmm0(code, 0);
  opcodes::store_xmm0_to_memory_by_rbx_offset(code, 0);

  opcodes::load_from_memory_by_rdi_offset_to_xmm0(code, 1);
  opcodes::sub_from_memory_by_rbx_offset_to_xmm0(code, 1);
  opcodes::store_xmm0_to_memory_by_rbx_offset(code, 1);

  opcodes::load_from_memory_by_rdi_offset_to_xmm0(code, 2);
  opcodes::mul_from_memory_by_rbx_offset_to_xmm0(code, 2);
  opcodes::store_xmm0_to_memory_by_rbx_offset(code, 2);

  opcodes::load_from_memory_by_rdi_offset_to_xmm0(code, 3);
  opcodes::div_from_memory_by_rbx_offset_to_xmm0(code, 3);
  opcodes::store_xmm0_to_memory_by_rbx_offset(code, 3);

  opcodes::load_from_memory_by_rbx_offset_to_xmm0(code, 0);
  opcodes::add_from_memory_by_rbx_offset_to_xmm0(code, 0);
  opcodes::store_xmm0_to_memory_by_rbx_offset(code, 4);

  opcodes::load_immediate_to_memory_by_rbx_offset(code, 5, 42.5);

  opcodes::pop_rbx(code);

  opcodes::ret(code);

  std::vector<double> x(4, 10.0);
  std::vector<double> y(6, 5.0);

  EXPECT_EQ(10.0, x[0]);
  EXPECT_EQ(10.0, x[1]);
  EXPECT_EQ(10.0, x[2]);
  EXPECT_EQ(10.0, x[3]);

  EXPECT_EQ(5.0, y[0]);
  EXPECT_EQ(5.0, y[1]);
  EXPECT_EQ(5.0, y[2]);
  EXPECT_EQ(5.0, y[3]);
  EXPECT_EQ(5.0, y[4]);
  EXPECT_EQ(5.0, y[5]);

  (current::fncas::linux_native_jit::CallableVectorUInt8(code))(&x[0], &y[0], nullptr);

  EXPECT_EQ(15.0, y[0]);
  EXPECT_EQ(5.0, y[1]);
  EXPECT_EQ(50.0, y[2]);
  EXPECT_EQ(2.0, y[3]);
  EXPECT_EQ(30.0, y[4]);  // `y[4]` is `y[0] + y[0]`, loaded by `rbx` instead of by `rsi`. -- D.K.
  EXPECT_EQ(42.5, y[5]);
}

TEST(LinuxNativeJIT, CallsExternalFunctions) {
  using namespace current::fncas::linux_native_jit;

  double (*f[])(double x) = {sin, cos, exp};

  std::vector<uint8_t> code;

  // HACK(dkorolev): Well, `cos` needs two extra elements on the stack to be accessible, otherwise it segfaults. -- D.K.
  opcodes::push_rbx(code);
  opcodes::push_rbx(code);
  opcodes::push_rbx(code);

  opcodes::mov_rsi_rbx(code);

  opcodes::load_from_memory_by_rdi_offset_to_xmm0(code, 0);
  opcodes::push_rdi(code);
  opcodes::push_rdx(code);
  opcodes::call_function_from_rdx_pointers_array_by_index(code, 0);
  opcodes::pop_rdx(code);
  opcodes::pop_rdi(code);
  opcodes::store_xmm0_to_memory_by_rbx_offset(code, 0);

  opcodes::load_from_memory_by_rdi_offset_to_xmm0(code, 1);
  opcodes::push_rdi(code);
  opcodes::push_rdx(code);
  opcodes::call_function_from_rdx_pointers_array_by_index(code, 1);
  opcodes::pop_rdx(code);
  opcodes::pop_rdi(code);
  opcodes::store_xmm0_to_memory_by_rbx_offset(code, 1);

  opcodes::load_from_memory_by_rdi_offset_to_xmm0(code, 1);
  opcodes::push_rdi(code);
  opcodes::push_rdx(code);
  opcodes::call_function_from_rdx_pointers_array_by_index(code, 2);
  opcodes::pop_rdx(code);
  opcodes::pop_rdi(code);
  opcodes::store_xmm0_to_memory_by_rbx_offset(code, 2);

  opcodes::pop_rbx(code);
  opcodes::pop_rbx(code);
  opcodes::pop_rbx(code);

  opcodes::ret(code);

  std::vector<double> x(3, 1.0);
  std::vector<double> y(3);

  (current::fncas::linux_native_jit::CallableVectorUInt8(code))(&x[0], &y[0], f);

  EXPECT_EQ(sin(1.0), y[0]);
  EXPECT_EQ(cos(1.0), y[1]);
  EXPECT_EQ(exp(1.0), y[2]);
}

#endif  // FNCAS_LINUX_NATIVE_JIT_ENABLED
