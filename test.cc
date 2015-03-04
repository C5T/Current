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

// TODO(dkorolev)+TODO(mzhurovich): Chat about this `typename fncas::output<T>::type` syntax. We can do better.
template <typename T>
typename fncas::output<T>::type f(const T& x) {
  return (x[0] + x[1] * 2) * (x[0] + x[1] * 2);
}

TEST(FNCAS, ReallyNativeComputationJustToBeSure) { EXPECT_EQ(25, f(std::vector<double>({1, 2}))); }

TEST(FNCAS, NativeWrapper) {
  fncas::reset_internals_singleton();
  fncas::f_native fn(f<std::vector<double>>, 2);
  EXPECT_EQ(25.0, fn(std::vector<double>({1.0, 2.0})));
}

TEST(FNCAS, IntermediateWrapper) {
  fncas::reset_internals_singleton();
  fncas::x x(2);
  fncas::f_intermediate fi = f(x);
  EXPECT_EQ(25.0, fi(std::vector<double>({1.0, 2.0})));
  EXPECT_EQ("((x[0]+(x[1]*2.000000))*(x[0]+(x[1]*2.000000)))", fi.debug_as_string());
}

TEST(FNCAS, CompilingWrapper) {
  fncas::reset_internals_singleton();
  fncas::x x(2);
  fncas::f_intermediate fi = f(x);
  fncas::f_compiled fc = fncas::f_compiled(fi);
  EXPECT_EQ(25.0, fc(std::vector<double>({1.0, 2.0}))) << fc.lib_filename();
}

TEST(FNCAS, GradientsWrapper) {
  fncas::reset_internals_singleton();
  auto p_3_3 = std::vector<double>({3.0, 3.0});

  fncas::g_approximate ga = fncas::g_approximate(f<std::vector<double>>, 2);
  auto d_3_3_approx = ga(p_3_3);
  EXPECT_EQ(81.0, d_3_3_approx.value);
  EXPECT_NEAR(18.0, d_3_3_approx.gradient[0], 1e-5);
  EXPECT_NEAR(36.0, d_3_3_approx.gradient[1], 1e-5);

  const fncas::x x(2);
  fncas::g_intermediate gi = fncas::g_intermediate(x, f(x));
  auto d_3_3_intermediate = gi(p_3_3);
  EXPECT_EQ(81.0, d_3_3_intermediate.value);
  EXPECT_EQ(18.0, d_3_3_intermediate.gradient[0]);
  EXPECT_EQ(36.0, d_3_3_intermediate.gradient[1]);
}

// Example gradient descent optimization code.
// TODO(mzhurovich): Move it to the right header file, use an optimized implementation and test in full pls.
struct OptimizationResult {
  double value;
  std::vector<double> point;
};
template<class F> OptimizationResult OptimizeUsingGradientDescent(const std::vector<double>& starting_point) {
  fncas::reset_internals_singleton();
  const size_t dim = starting_point.size();
  const fncas::x gradient_helper(dim);
  fncas::f_intermediate fi(F::compute(gradient_helper));
  fncas::g_intermediate gi = fncas::g_intermediate(gradient_helper, fi);
  std::vector<double> current_point(starting_point);
  for (size_t iteration = 0; iteration < 5000; ++iteration) {
    const auto g = gi(current_point);
    const auto try_step = [&dim, &current_point, &fi, &g](double step) -> std::pair<double, std::vector<double>> {
      std::vector<double> candidate(dim);
      for (size_t i = 0; i < dim; ++i) {
        candidate[i] = current_point[i] - g.gradient[i] * step;
      }
      const double value = fi(candidate);
      return std::make_pair(value, candidate);
    };
    current_point = std::min(try_step(0.01), std::min(try_step(0.05), try_step(0.2))).second;
  }
  return OptimizationResult{fi(current_point), current_point};
}
// TODO(mzhurovich): Please merge these two implementations to avoid copy-pasting.
template<class F> OptimizationResult OptimizeUsingGradientDescent(F&& user_f, const std::vector<double>& starting_point) {
  fncas::reset_internals_singleton();
  const size_t dim = starting_point.size();
  const fncas::x gradient_helper(dim);
  fncas::f_intermediate fi(user_f(gradient_helper));
  fncas::g_intermediate gi = fncas::g_intermediate(gradient_helper, fi);
  std::vector<double> current_point(starting_point);
  for (size_t iteration = 0; iteration < 5000; ++iteration) {
    const auto g = gi(current_point);
    const auto try_step = [&dim, &current_point, &fi, &g](double step) -> std::pair<double, std::vector<double>> {
      std::vector<double> candidate(dim);
      for (size_t i = 0; i < dim; ++i) {
        candidate[i] = current_point[i] - g.gradient[i] * step;
      }
      const double value = fi(candidate);
      return std::make_pair(value, candidate);
    };
    current_point = std::min(try_step(0.01), std::min(try_step(0.05), try_step(0.2))).second;
  }
  return OptimizationResult{fi(current_point), current_point};
}

struct StaticFunctionToOptimize {
  template <typename T>
  static typename fncas::output<T>::type compute(const T& x) {
    // An obviously convex function with a single minimum `f(3, 4) == 1`.
    const auto dx = x[0] - 3;
    const auto dy = x[1] - 4;
    return exp(0.01 * (dx * dx + dy * dy));
  }
};

struct MemberFunctionToOptimize {
  double a = 0.0;
  double b = 0.0;
  template <typename T>
  typename fncas::output<T>::type operator()(const T& x) {
    // An obviously convex function with a single minimum `f(a, b) == 1`.
    const auto dx = x[0] - a;
    const auto dy = x[1] - b;
    return exp(0.01 * (dx * dx + dy * dy));
  }
};

TEST(FNCAS, OptimizationOfAStaticFunction) {
  const auto result = OptimizeUsingGradientDescent<StaticFunctionToOptimize>(std::vector<double>({0, 0}));
  EXPECT_NEAR(1.0, result.value, 1e-3);
  ASSERT_EQ(2u, result.point.size());
  EXPECT_NEAR(3.0, result.point[0], 1e-3);
  EXPECT_NEAR(4.0, result.point[1], 1e-3);
}

TEST(FNCAS, OptimizationOfAMemberFunction) {
  MemberFunctionToOptimize f;
  f.a = 2.0;
  f.b = 1.0;
  const auto result = OptimizeUsingGradientDescent(f, std::vector<double>({0, 0}));
  EXPECT_NEAR(1.0, result.value, 1e-3);
  ASSERT_EQ(2u, result.point.size());
  EXPECT_NEAR(2.0, result.point[0], 1e-3);
  EXPECT_NEAR(1.0, result.point[1], 1e-3);
}
