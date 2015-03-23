/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#include "../Bricks/3party/gtest/gtest-main.h"

#ifndef FNCAS_JIT
#define FNCAS_JIT CLANG
#endif

#include "fncas/fncas.h"

#include <functional>
#include <thread>

// TODO(dkorolev)+TODO(mzhurovich): Chat about this `typename fncas::output<T>::type` syntax. We can do better.
template <typename T>
typename fncas::output<T>::type parametrized_f(const T& x, size_t c) {
  return (x[0] + x[1] * c) * (x[0] + x[1] * c);
}

// Need an explicit specialization, not a default parameter, since `f` itself is used as a parameter later on.
template <typename T>
typename fncas::output<T>::type f(const T& x) {
  return parametrized_f(x, 2u);
}

TEST(FNCAS, ReallyNativeComputationJustToBeSure) { EXPECT_EQ(25, f(std::vector<double>({1, 2}))); }

TEST(FNCAS, NativeWrapper) {
  fncas::reset_internals_singleton();
  fncas::f_native fn(f<std::vector<double>>, 2);
  EXPECT_EQ(25.0, fn({1.0, 2.0}));
}

TEST(FNCAS, IntermediateWrapper) {
  fncas::reset_internals_singleton();
  fncas::x x(2);
  fncas::f_intermediate fi = f(x);
  EXPECT_EQ(25.0, fi({1.0, 2.0}));
  EXPECT_EQ("((x[0]+(x[1]*2.000000))*(x[0]+(x[1]*2.000000)))", fi.debug_as_string());
}

TEST(FNCAS, CompilingWrapper) {
  fncas::reset_internals_singleton();
  fncas::x x(2);
  fncas::f_intermediate fi = f(x);
  fncas::f_compiled fc = fncas::f_compiled(fi);
  EXPECT_EQ(25.0, fc({1.0, 2.0})) << fc.lib_filename();
}

TEST(FNCAS, GradientsWrapper) {
  fncas::reset_internals_singleton();
  std::vector<double> p_3_3({3.0, 3.0});

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

TEST(FNCAS, SupportsConcurrentThreadsViaThreadLocal) {
  const auto advanced_math = [](){
    for (size_t i = 0; i < 1000; ++i) {
      fncas::reset_internals_singleton();
      fncas::x x(2);
      fncas::f_intermediate fi = parametrized_f(x, i + 1);
      EXPECT_EQ(sqr(1.0 + 2.0 * (i + 1)), fi({1.0, 2.0}));
    }
  };
  std::thread t1(advanced_math);
  std::thread t2(advanced_math);
  std::thread t3(advanced_math);
  t1.join();
  t2.join();
  t3.join();
}

// An obviously convex function with a single minimum `f(3, 4) == 1`.
struct StaticFunction {
  template <typename T>
  static typename fncas::output<T>::type compute(const T& x) {
    const auto dx = x[0] - 3;
    const auto dy = x[1] - 4;
    return exp(0.01 * (dx * dx + dy * dy));
  }
};

/*
// TODO(dkorolev): Re-add support for member functions optimization.
// An obviously convex function with a single minimum `f(a, b) == 1`.
struct MemberFunction {
  double a = 0.0;
  double b = 0.0;
  template <typename T>
  typename fncas::output<T>::type operator()(const T& x) {
    const auto dx = x[0] - a;
    const auto dy = x[1] - b;
    return exp(0.01 * (dx * dx + dy * dy));
  }
};
*/

// An obviously convex function with a single minimum `f(0, 0) == 0`.
struct PolynomialFunction {
  template <typename T>
  static typename fncas::output<T>::type compute(const T& x) {
    const double a = 10.0;
    const double b = 0.5;
    return (a * x[0] * x[0] + b * x[1] * x[1]);
  }
};

// http://en.wikipedia.org/wiki/Rosenbrock_function
// Non-convex function with global minimum `f(a, a^2) == 0`.
struct RosenbrockFunction {
  template <typename T>
  static typename fncas::output<T>::type compute(const T& x) {
    const double a = 1.0;
    const double b = 100.0;
    const auto d1 = (a - x[0]);
    const auto d2 = (x[1] - x[0] * x[0]);
    return (d1 * d1 + b * d2 * d2);
  }
};

// http://en.wikipedia.org/wiki/Himmelblau%27s_function
// Non-convex function with four local minima:
// f(3.0, 2.0) = 0.0
// f(-2.805118, 3.131312) = 0.0
// f(-3.779310, -3.283186) = 0.0
// f(3.584428, -1.848126) = 0.0
struct HimmelblauFunction {
  template <typename T>
  static typename fncas::output<T>::type compute(const T& x) {
    const auto d1 = (x[0] * x[0] + x[1] - 11);
    const auto d2 = (x[0] + x[1] * x[1] - 7);
    return (d1 * d1 + d2 * d2);
  }
};

TEST(FNCAS, OptimizationOfAStaticFunction) {
  const auto result = fncas::GradientDescentOptimizer<StaticFunction>().Optimize({0, 0});
  EXPECT_NEAR(1.0, result.value, 1e-3);
  ASSERT_EQ(2u, result.point.size());
  EXPECT_NEAR(3.0, result.point[0], 1e-3);
  EXPECT_NEAR(4.0, result.point[1], 1e-3);
}

/*
// TODO(dkorolev): Re-add support for member functions optimization.
TEST(FNCAS, OptimizationOfAMemberFunction) {
  MemberFunction f;
  f.a = 2.0;
  f.b = 1.0;
  const auto result = fncas::OptimizeUsingGradientDescent(f, {0, 0}));
  EXPECT_NEAR(1.0, result.value, 1e-3);
  ASSERT_EQ(2u, result.point.size());
  EXPECT_NEAR(2.0, result.point[0], 1e-3);
  EXPECT_NEAR(1.0, result.point[1], 1e-3);
}
*/

TEST(FNCAS, OptimizationOfAPolynomialMemberFunction) {
  const auto result = fncas::GradientDescentOptimizer<PolynomialFunction>().Optimize({5.0, 20.0});
  EXPECT_NEAR(0.0, result.value, 1e-3);
  ASSERT_EQ(2u, result.point.size());
  EXPECT_NEAR(0.0, result.point[0], 1e-3);
  EXPECT_NEAR(0.0, result.point[1], 1e-3);
}

TEST(FNCAS, OptimizationOfAPolynomialUsingBacktrackingGD) {
  const auto result = fncas::GradientDescentOptimizerBT<PolynomialFunction>().Optimize({5.0, 20.0});
  EXPECT_NEAR(0.0, result.value, 1e-3);
  ASSERT_EQ(2u, result.point.size());
  EXPECT_NEAR(0.0, result.point[0], 1e-3);
  EXPECT_NEAR(0.0, result.point[1], 1e-3);
}

TEST(FNCAS, OptimizationOfAPolynomialUsingConjugateGradient) {
  const auto result = fncas::ConjugateGradientOptimizer<PolynomialFunction>().Optimize({5.0, 20.0});
  EXPECT_NEAR(0.0, result.value, 1e-6);
  ASSERT_EQ(2u, result.point.size());
  EXPECT_NEAR(0.0, result.point[0], 1e-6);
  EXPECT_NEAR(0.0, result.point[1], 1e-6);
}

TEST(FNCAS, OptimizationOfRosenbrockUsingConjugateGradient) {
  const auto result = fncas::ConjugateGradientOptimizer<RosenbrockFunction>().Optimize({-3.0, -4.0});
  EXPECT_NEAR(0.0, result.value, 1e-6);
  ASSERT_EQ(2u, result.point.size());
  EXPECT_NEAR(1.0, result.point[0], 1e-6);
  EXPECT_NEAR(1.0, result.point[1], 1e-6);
}

TEST(FNCAS, OptimizationOfHimmelbaluUsingCojugateGradient) {
  fncas::ConjugateGradientOptimizer<HimmelblauFunction> optimizer;
  const auto min1 = optimizer.Optimize({5.0, 5.0});
  EXPECT_NEAR(0.0, min1.value, 1e-6);
  ASSERT_EQ(2u, min1.point.size());
  EXPECT_NEAR(3.0, min1.point[0], 1e-6);
  EXPECT_NEAR(2.0, min1.point[1], 1e-6);

  const auto min2 = optimizer.Optimize({-3.0, 5.0});
  EXPECT_NEAR(0.0, min2.value, 1e-6);
  ASSERT_EQ(2u, min2.point.size());
  EXPECT_NEAR(-2.805118, min2.point[0], 1e-6);
  EXPECT_NEAR(3.131312, min2.point[1], 1e-6);

  const auto min3 = optimizer.Optimize({-5.0, -5.0});
  EXPECT_NEAR(0.0, min3.value, 1e-6);
  ASSERT_EQ(2u, min3.point.size());
  EXPECT_NEAR(-3.779310, min3.point[0], 1e-6);
  EXPECT_NEAR(-3.283186, min3.point[1], 1e-6);

  const auto min4 = optimizer.Optimize({5.0, -5.0});
  EXPECT_NEAR(0.0, min4.value, 1e-6);
  ASSERT_EQ(2u, min4.point.size());
  EXPECT_NEAR(3.584428, min4.point[0], 1e-6);
  EXPECT_NEAR(-1.848126, min4.point[1], 1e-6);
}

// Check that gradient descent optimizer with backtracking performs better than
// naive optimizer on Rosenbrock function, when maximum step count = 1000.
TEST(FNCAS, NaiveGDvsBacktrackingGDOnRosenbrockFunction1000Steps) {
  fncas::OptimizerParameters params;
  params.SetValue("max_steps", 1000);
  params.SetValue("step_factor", 0.001);  // Used only by naive optimizer. Prevents it from moving to infinity.
  const auto result_naive = fncas::GradientDescentOptimizer<RosenbrockFunction>(params).Optimize({-3.0, -4.0});
  const auto result_bt = fncas::GradientDescentOptimizerBT<RosenbrockFunction>(params).Optimize({-3.0, -4.0});
  const double x0_err_n = std::abs(result_naive.point[0] - 1.0);
  const double x0_err_bt = std::abs(result_bt.point[0] - 1.0);
  const double x1_err_n = std::abs(result_naive.point[1] - 1.0);
  const double x1_err_bt = std::abs(result_bt.point[1] - 1.0);
  ASSERT_TRUE(fncas::IsNormal(x0_err_n));
  ASSERT_TRUE(fncas::IsNormal(x1_err_n));
  ASSERT_TRUE(fncas::IsNormal(x0_err_bt));
  ASSERT_TRUE(fncas::IsNormal(x1_err_bt));
  EXPECT_TRUE(x0_err_bt < x0_err_n);
  EXPECT_TRUE(x1_err_bt < x1_err_n);
  EXPECT_NEAR(1.0, result_bt.point[0], 1e-6);
  EXPECT_NEAR(1.0, result_bt.point[1], 1e-6);
}

// Check that conjugate gradient optimizer performs better than gradient descent
// optimizer with backtracking on Rosenbrock function, when maximum step count = 100.
TEST(FNCAS, ConjugateGDvsBacktrackingGDOnRosenbrockFunction100Steps) {
  fncas::OptimizerParameters params;
  params.SetValue("max_steps", 100);
  const auto result_cg = fncas::ConjugateGradientOptimizer<RosenbrockFunction>(params).Optimize({-3.0, -4.0});
  const auto result_bt = fncas::GradientDescentOptimizerBT<RosenbrockFunction>(params).Optimize({-3.0, -4.0});
  const double x0_err_cg = std::abs(result_cg.point[0] - 1.0);
  const double x0_err_bt = std::abs(result_bt.point[0] - 1.0);
  const double x1_err_cg = std::abs(result_cg.point[1] - 1.0);
  const double x1_err_bt = std::abs(result_bt.point[1] - 1.0);
  ASSERT_TRUE(fncas::IsNormal(x0_err_cg));
  ASSERT_TRUE(fncas::IsNormal(x1_err_cg));
  ASSERT_TRUE(fncas::IsNormal(x0_err_bt));
  ASSERT_TRUE(fncas::IsNormal(x1_err_bt));
  EXPECT_TRUE(x0_err_cg < x0_err_bt);
  EXPECT_TRUE(x1_err_cg < x1_err_bt);
  EXPECT_NEAR(1.0, result_cg.point[0], 1e-6);
  EXPECT_NEAR(1.0, result_cg.point[1], 1e-6);
}
