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

#include "Bricks/3party/gtest/gtest-main.h"

#ifndef FNCAS_JIT
#define FNCAS_JIT CLANG
#endif

#include "fncas/fncas.h"

template <typename T>
typename fncas::output<T>::type f(const T& x) {
  return (x[0] + x[1] * 2) * (x[0] + x[1] * 2);
}

TEST(FNCAS, ReallyNativeComputationJustToBeSure) { EXPECT_EQ(25, f(std::vector<double>({1, 2}))); }

TEST(FNCAS, NativeWrapper) {
  fncas::reset_internals_singleton();
  fncas::f_native fn(f<std::vector<double>>, 2);
  EXPECT_EQ(25, fn(std::vector<double>({1, 2})));
}

TEST(FNCAS, IntermediateWrapper) {
  fncas::reset_internals_singleton();
  fncas::x x(2);
  fncas::f_intermediate fi = f(x);
  EXPECT_EQ(25, fi(std::vector<double>({1, 2})));
  EXPECT_EQ("((x[0]+(x[1]*2.000000))*(x[0]+(x[1]*2.000000)))", fi.debug_as_string());
}

TEST(FNCAS, CompilingWrapper) {
  fncas::reset_internals_singleton();
  fncas::x x(2);
  fncas::f_intermediate fi = f(x);
  fncas::f_compiled fc = fncas::f_compiled(fi);
  EXPECT_EQ(25, fc(std::vector<double>({1, 2}))) << fc.lib_filename();
}

TEST(FNCAS, GradientsWrapper) {
  fncas::reset_internals_singleton();
  auto p_3_3 = std::vector<double>({3, 3});

  fncas::g_approximate ga = fncas::g_approximate(f<std::vector<double>>, 2);
  auto d_3_3_approx = ga(p_3_3);
  EXPECT_EQ(81, d_3_3_approx.value);
  // TODO(dkorolev): Approximate comparison once I'm online to Google it.
  if (false) {
    EXPECT_EQ(18, d_3_3_approx.gradient[0]);
    EXPECT_EQ(0, d_3_3_approx.gradient[1]);
  }

  const fncas::x x(2);
  fncas::g_intermediate gi = fncas::g_intermediate(x, f(x));
  auto d_3_3_intermediate = gi(p_3_3);
  EXPECT_EQ(81, d_3_3_intermediate.value);
  EXPECT_EQ(18, d_3_3_intermediate.gradient[0]);
  EXPECT_EQ(36, d_3_3_intermediate.gradient[1]);
}
