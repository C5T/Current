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

#include "../3rdparty/gtest/gtest-main.h"

// #define FNCAS_USE_LONG_DOUBLE

#ifndef FNCAS_USE_LONG_DOUBLE
#ifndef FNCAS_JIT
// Another supported value is `NASM`. -- D.K.
#define FNCAS_JIT CLANG
#endif  // #ifndef FNCAS_JI
#endif  // #ifndef FNCAS_USE_LONG_DOUBLE

#include "fncas/fncas.h"

#include <functional>
#include <thread>

template <typename X>
inline fncas::X2V<X> ParametrizedFunction(const X& x, size_t c) {
  return (x[0] + x[1] * c) * (x[0] + x[1] * c);
}

// Need an explicit specialization, as `SimpleFunction<std::vector<double_t>>` is used directly in the test.
template <typename X>
inline fncas::X2V<X> SimpleFunction(const X& x) {
  return ParametrizedFunction(x, 2u);
}

static_assert(std::is_same<std::vector<fncas::double_t>, fncas::V2X<fncas::double_t>>::value, "");
static_assert(std::is_same<fncas::X2V<std::vector<fncas::double_t>>, fncas::double_t>::value, "");

static_assert(std::is_same<fncas::V, fncas::X2V<fncas::X>>::value, "");
static_assert(std::is_same<fncas::X, fncas::V2X<fncas::V>>::value, "");

TEST(FnCAS, ReallyNativeComputationJustToBeSure) {
  EXPECT_EQ(25, SimpleFunction(std::vector<fncas::double_t>({1, 2})));
}

TEST(FnCAS, NativeWrapper) {
  fncas::f_native fn(SimpleFunction<std::vector<fncas::double_t>>, 2);
  EXPECT_EQ(25.0, fn({1.0, 2.0}));
}

TEST(FnCAS, IntermediateWrapper) {
  fncas::X x(2);
  fncas::f_intermediate fi = SimpleFunction(x);
  EXPECT_EQ(25.0, fi({1.0, 2.0}));
  EXPECT_EQ("((x[0]+(x[1]*2.000000))*(x[0]+(x[1]*2.000000)))", fi.debug_as_string());
}

#ifdef FNCAS_JIT
TEST(FnCAS, CompiledFunctionWrapper) {
  fncas::X x(2);
  fncas::f_intermediate fi = SimpleFunction(x);
  fncas::f_compiled fc = fncas::f_compiled(fi);
  EXPECT_EQ(25.0, fc({1.0, 2.0})) << fc.lib_filename();
}
#endif  // FNCAS_JIT

TEST(FnCAS, GradientsWrapper) {
  std::vector<fncas::double_t> p_3_3({3.0, 3.0});

  fncas::g_approximate ga = fncas::g_approximate(SimpleFunction<std::vector<fncas::double_t>>, 2);
  auto d_3_3_approx = ga(p_3_3);
  EXPECT_NEAR(18.0, d_3_3_approx[0], 1e-5);
  EXPECT_NEAR(36.0, d_3_3_approx[1], 1e-5);

  const fncas::X x(2);
  const fncas::g_intermediate gi(x, SimpleFunction(x));
  const auto d_3_3_intermediate = gi(p_3_3);
  EXPECT_EQ(18, d_3_3_intermediate[0]);
  EXPECT_EQ(36, d_3_3_intermediate[1]);
}

#ifdef FNCAS_JIT
TEST(FnCAS, CompiledGradientsWrapper) {
  std::vector<fncas::double_t> p_3_3({3.0, 3.0});

  const fncas::X x(2);
  const fncas::f_intermediate fi = SimpleFunction(x);
  const fncas::g_intermediate gi(x, fi);

  const fncas::g_compiled gc(fi, gi);

  // TODO(dkorolev): Maybe return return function value and its gradient together from a call to `gc`?
  const auto d_3_3_compiled = gc(p_3_3);

  EXPECT_EQ(18, d_3_3_compiled[0]) << gc.lib_filename();
  EXPECT_EQ(36, d_3_3_compiled[1]) << gc.lib_filename();
}

TEST(FnCAS, CompiledSqrGradientWrapper) {
  // The `sqr()` function is a special case, which it worth unit-testing with different `FNCAS_JIT. -- D.K.
  std::vector<fncas::double_t> p_3_3({3.0, 3.0});

  const fncas::X x(2);
  const fncas::f_intermediate fi = SimpleFunction(x);
  const fncas::g_intermediate gi(x, SimpleFunction(x));

  const fncas::f_compiled fc(fi);
  const fncas::double_t f_3_3_compiled = fc(p_3_3);
  EXPECT_EQ(81, f_3_3_compiled);

  const fncas::g_compiled gc(fi, gi);
  const auto d_3_3_compiled = gc(p_3_3);

  EXPECT_EQ(18, d_3_3_compiled[0]) << gc.lib_filename();
  EXPECT_EQ(36, d_3_3_compiled[1]) << gc.lib_filename();
}
#endif  // FNCAS_JIT

TEST(FnCAS, SupportsConcurrentThreadsViaThreadLocal) {
  const auto advanced_math = []() {
    for (size_t i = 0; i < 1000; ++i) {
      fncas::X x(2);
      fncas::f_intermediate fi = ParametrizedFunction(x, i + 1);
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

TEST(FnCAS, CannotEvaluateMoreThanOneFunctionPerThreadAtOnce) {
  fncas::X x(1);
  ASSERT_THROW(fncas::X x(2), fncas::FnCASConcurrentEvaluationAttemptException);
}

// An obviously convex function with a single minimum `f(3, 4) == 1`.
struct StaticFunction {
  template <typename X>
  static fncas::X2V<X> ObjectiveFunction(const X& x) {
    const auto dx = x[0] - 3;
    const auto dy = x[1] - 4;
    return exp(0.01 * (dx * dx + dy * dy));
  }
};

// An obviously convex function with a single minimum `f(a, b) == 1`.
struct MemberFunction {
  fncas::double_t a = 0.0;
  fncas::double_t b = 0.0;
  template <typename T>
  typename fncas::X2V<T> ObjectiveFunction(const T& x) const {
    const auto dx = x[0] - a;
    const auto dy = x[1] - b;
    return exp(0.01 * (dx * dx + dy * dy));
  }
  MemberFunction() = default;
  MemberFunction(const MemberFunction&) = delete;
};

struct MemberFunctionWithReferences {
  fncas::double_t& a;
  fncas::double_t& b;
  MemberFunctionWithReferences(fncas::double_t& a, fncas::double_t& b) : a(a), b(b) {}
  template <typename T>
  typename fncas::X2V<T> ObjectiveFunction(const T& x) const {
    const auto dx = x[0] - a;
    const auto dy = x[1] - b;
    return exp(0.01 * (dx * dx + dy * dy));
  }
  MemberFunctionWithReferences() = default;
  MemberFunctionWithReferences(const MemberFunctionWithReferences&) = delete;
};

// An obviously convex function with a single minimum `f(0, 0) == 0`.
struct PolynomialFunction {
  template <typename X>
  fncas::X2V<X> ObjectiveFunction(const X& x) const {
    const fncas::double_t a = 10.0;
    const fncas::double_t b = 0.5;
    return (a * x[0] * x[0] + b * x[1] * x[1]);
  }
};

// http://en.wikipedia.org/wiki/Rosenbrock_function
// Non-convex function with global minimum `f(a, a^2) == 0`.
struct RosenbrockFunction {
  template <typename X>
  fncas::X2V<X> ObjectiveFunction(const X& x) const {
    const fncas::double_t a = 1.0;
    const fncas::double_t b = 100.0;
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
  template <typename X>
  fncas::X2V<X> ObjectiveFunction(const X& x) const {
    const auto d1 = (x[0] * x[0] + x[1] - 11);
    const auto d2 = (x[0] + x[1] * x[1] - 7);
    return (d1 * d1 + d2 * d2);
  }
};

TEST(FnCAS, OptimizationOfAStaticFunction) {
  const auto result = fncas::GradientDescentOptimizer<StaticFunction>().Optimize({0, 0});
  EXPECT_NEAR(1.0, result.value, 1e-3);
  ASSERT_EQ(2u, result.point.size());
  EXPECT_NEAR(3.0, result.point[0], 1e-3);
  EXPECT_NEAR(4.0, result.point[1], 1e-3);
}

TEST(FnCAS, OptimizationOfAMemberFunctionSmokeTest) {
  MemberFunction f;
  {
    f.a = 2.0;
    f.b = 1.0;
    const auto result = fncas::GradientDescentOptimizer<MemberFunction>(f).Optimize({0, 0});
    EXPECT_NEAR(1.0, result.value, 1e-3);
    ASSERT_EQ(2u, result.point.size());
    EXPECT_NEAR(2.0, result.point[0], 1e-3);
    EXPECT_NEAR(1.0, result.point[1], 1e-3);
  }
  {
    f.a = 3.0;
    f.b = 4.0;
    const auto result = fncas::GradientDescentOptimizer<MemberFunction>(f).Optimize({0, 0});
    EXPECT_NEAR(1.0, result.value, 1e-3);
    ASSERT_EQ(2u, result.point.size());
    EXPECT_NEAR(3.0, result.point[0], 1e-3);
    EXPECT_NEAR(4.0, result.point[1], 1e-3);
  }
}

TEST(FnCAS, OptimizationOfAMemberFunctionCapturesFunctionByReference) {
  MemberFunction f;
  fncas::GradientDescentOptimizer<MemberFunction> optimizer(f);
  {
    f.a = 2.0;
    f.b = 1.0;
    const auto result = optimizer.Optimize({0, 0});
    EXPECT_NEAR(1.0, result.value, 1e-3);
    ASSERT_EQ(2u, result.point.size());
    EXPECT_NEAR(2.0, result.point[0], 1e-3);
    EXPECT_NEAR(1.0, result.point[1], 1e-3);
  }
  {
    f.a = 3.0;
    f.b = 4.0;
    const auto result = optimizer.Optimize({0, 0});
    EXPECT_NEAR(1.0, result.value, 1e-3);
    ASSERT_EQ(2u, result.point.size());
    EXPECT_NEAR(3.0, result.point[0], 1e-3);
    EXPECT_NEAR(4.0, result.point[1], 1e-3);
  }
}

TEST(FnCAS, OptimizationOfAMemberFunctionConstructsObjectiveFunctionObject) {
  fncas::GradientDescentOptimizer<MemberFunction> optimizer;  // Will construct the object by itself.
  {
    optimizer->a = 2.0;
    optimizer->b = 1.0;
    const auto result = optimizer.Optimize({0, 0});
    EXPECT_NEAR(1.0, result.value, 1e-3);
    ASSERT_EQ(2u, result.point.size());
    EXPECT_NEAR(2.0, result.point[0], 1e-3);
    EXPECT_NEAR(1.0, result.point[1], 1e-3);
  }
  {
    optimizer->a = 3.0;
    optimizer->b = 4.0;
    const auto result = optimizer.Optimize({0, 0});
    EXPECT_NEAR(1.0, result.value, 1e-3);
    ASSERT_EQ(2u, result.point.size());
    EXPECT_NEAR(3.0, result.point[0], 1e-3);
    EXPECT_NEAR(4.0, result.point[1], 1e-3);
  }
}

TEST(FnCAS, OptimizationOfAMemberFunctionForwardsParameters) {
  fncas::double_t a = 0;
  fncas::double_t b = 0;
  // `GradientDescentOptimizer` will construct the instance of `MemberFunctionWithReferences`.
  fncas::GradientDescentOptimizer<MemberFunctionWithReferences> optimizer(a, b);
  {
    a = 2.0;
    b = 1.0;
    const auto result = optimizer.Optimize({0, 0});
    EXPECT_NEAR(1.0, result.value, 1e-3);
    ASSERT_EQ(2u, result.point.size());
    EXPECT_NEAR(2.0, result.point[0], 1e-3);
    EXPECT_NEAR(1.0, result.point[1], 1e-3);
  }
  {
    a = 3.0;
    b = 4.0;
    const auto result = optimizer.Optimize({0, 0});
    EXPECT_NEAR(1.0, result.value, 1e-3);
    ASSERT_EQ(2u, result.point.size());
    EXPECT_NEAR(3.0, result.point[0], 1e-3);
    EXPECT_NEAR(4.0, result.point[1], 1e-3);
  }
}

TEST(FnCAS, OptimizationOfAPolynomialMemberFunction) {
  const auto result = fncas::GradientDescentOptimizer<PolynomialFunction>().Optimize({5.0, 20.0});
  EXPECT_NEAR(0.0, result.value, 1e-3);
  ASSERT_EQ(2u, result.point.size());
  EXPECT_NEAR(0.0, result.point[0], 1e-3);
  EXPECT_NEAR(0.0, result.point[1], 1e-3);
}

TEST(FnCAS, OptimizationOfAPolynomialUsingBacktrackingGD) {
  const auto result = fncas::GradientDescentOptimizerBT<PolynomialFunction>().Optimize({5.0, 20.0});
  EXPECT_NEAR(0.0, result.value, 1e-3);
  ASSERT_EQ(2u, result.point.size());
  EXPECT_NEAR(0.0, result.point[0], 1e-3);
  EXPECT_NEAR(0.0, result.point[1], 1e-3);
}

TEST(FnCAS, OptimizationOfAPolynomialUsingConjugateGradient) {
  const auto result = fncas::ConjugateGradientOptimizer<PolynomialFunction>().Optimize({5.0, 20.0});
  EXPECT_NEAR(0.0, result.value, 1e-6);
  ASSERT_EQ(2u, result.point.size());
  EXPECT_NEAR(0.0, result.point[0], 1e-6);
  EXPECT_NEAR(0.0, result.point[1], 1e-6);
}

TEST(FnCAS, OptimizationOfRosenbrockUsingConjugateGradient) {
  const auto result = fncas::ConjugateGradientOptimizer<RosenbrockFunction>().Optimize({-3.0, -4.0});
  EXPECT_NEAR(0.0, result.value, 1e-6);
  ASSERT_EQ(2u, result.point.size());
  EXPECT_NEAR(1.0, result.point[0], 1e-6);
  EXPECT_NEAR(1.0, result.point[1], 1e-6);
}

TEST(FnCAS, OptimizationOfHimmelblauUsingConjugateGradient) {
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
TEST(FnCAS, NaiveGDvsBacktrackingGDOnRosenbrockFunction1000Steps) {
  fncas::OptimizerParameters params;
  params.SetValue("max_steps", 1000);
  params.SetValue("step_factor", 0.001);  // Used only by naive optimizer. Prevents it from moving to infinity.
  const auto result_naive = fncas::GradientDescentOptimizer<RosenbrockFunction>(params).Optimize({-3.0, -4.0});
  const auto result_bt = fncas::GradientDescentOptimizerBT<RosenbrockFunction>(params).Optimize({-3.0, -4.0});
  const fncas::double_t x0_err_n = std::abs(result_naive.point[0] - 1.0);
  const fncas::double_t x0_err_bt = std::abs(result_bt.point[0] - 1.0);
  const fncas::double_t x1_err_n = std::abs(result_naive.point[1] - 1.0);
  const fncas::double_t x1_err_bt = std::abs(result_bt.point[1] - 1.0);
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
TEST(FnCAS, ConjugateGDvsBacktrackingGDOnRosenbrockFunction100Steps) {
  fncas::OptimizerParameters params;
  params.SetValue("max_steps", 100);
  const auto result_cg = fncas::ConjugateGradientOptimizer<RosenbrockFunction>(params).Optimize({-3.0, -4.0});
  const auto result_bt = fncas::GradientDescentOptimizerBT<RosenbrockFunction>(params).Optimize({-3.0, -4.0});
  const fncas::double_t x0_err_cg = std::abs(result_cg.point[0] - 1.0);
  const fncas::double_t x0_err_bt = std::abs(result_bt.point[0] - 1.0);
  const fncas::double_t x1_err_cg = std::abs(result_cg.point[1] - 1.0);
  const fncas::double_t x1_err_bt = std::abs(result_bt.point[1] - 1.0);
  ASSERT_TRUE(fncas::IsNormal(x0_err_cg));
  ASSERT_TRUE(fncas::IsNormal(x1_err_cg));
  ASSERT_TRUE(fncas::IsNormal(x0_err_bt));
  ASSERT_TRUE(fncas::IsNormal(x1_err_bt));
  EXPECT_TRUE(x0_err_cg < x0_err_bt);
  EXPECT_TRUE(x1_err_cg < x1_err_bt);
  EXPECT_NEAR(1.0, result_cg.point[0], 1e-6);
  EXPECT_NEAR(1.0, result_cg.point[1], 1e-6);
}

// To test evaluation and differentiation.
template <typename X>
inline fncas::X2V<X> ZeroOrXFunction(const X& x) {
  EXPECT_EQ(1u, x.size());
  return zero_or_x(x[0]);
}

// To test evaluation and differentiation of `f(g(x))` where `f` is `zero_or_x`.
template <typename X>
inline fncas::X2V<X> ZeroOrXOfSquareXMinusTen(const X& x) {
  EXPECT_EQ(1u, x.size());
  return zero_or_x(sqr(x[0]) - 10);  // So that the argument is sometimes negative.
}

TEST(FnCAS, CustomFunctions) {
  EXPECT_EQ(0.0, zero_or_one(-1.0));
  EXPECT_EQ(1.0, zero_or_one(+2.0));

  EXPECT_EQ(0.0, zero_or_x(-3.0));
  EXPECT_EQ(4.0, zero_or_x(+4.0));

  const fncas::X x(1);
  const fncas::f_intermediate intermediate_function = ZeroOrXFunction(x);
  EXPECT_EQ(0.0, intermediate_function({-5.0}));
  EXPECT_EQ(6.0, intermediate_function({+6.0}));

  fncas::f_compiled compiled_function(intermediate_function);
  EXPECT_EQ(0.0, compiled_function({-5.5})) << compiled_function.lib_filename();
  EXPECT_EQ(6.5, compiled_function({+6.5})) << compiled_function.lib_filename();

  const fncas::g_approximate approximate_gradient(ZeroOrXFunction<std::vector<fncas::double_t>>, 1);
  EXPECT_NEAR(0.0, approximate_gradient({-5.0})[0], 1e-6);
  EXPECT_NEAR(1.0, approximate_gradient({+6.0})[0], 1e-6);

  const fncas::g_intermediate intermediate_gradient(x, intermediate_function);
  EXPECT_EQ(0.0, intermediate_gradient({-7.0})[0]);
  EXPECT_EQ(1.0, intermediate_gradient({+8.0})[0]);

  fncas::g_compiled compiled_gradient(intermediate_function, intermediate_gradient);
  EXPECT_EQ(0.0, compiled_gradient({-9.5})[0]) << compiled_gradient.lib_filename();
  EXPECT_EQ(1.0, compiled_gradient({+9.5})[0]) << compiled_gradient.lib_filename();
}

TEST(FnCAS, ComplexCustomFunctions) {
  const fncas::X x(1);

  const fncas::f_intermediate intermediate_function = ZeroOrXOfSquareXMinusTen(x);
  EXPECT_EQ(0.0, intermediate_function({3.0}));  // zero_or_x(3*3 - 10) == 0
  EXPECT_EQ(6.0, intermediate_function({4.0}));  // zero_or_x(4*4 - 10) == 6

  fncas::f_compiled compiled_function(intermediate_function);
  EXPECT_EQ(0.0, compiled_function({3.0})) << compiled_function.lib_filename();
  EXPECT_EQ(6.0, compiled_function({4.0})) << compiled_function.lib_filename();

  const fncas::g_approximate approximate_gradient(ZeroOrXOfSquareXMinusTen<std::vector<fncas::double_t>>, 1);
  EXPECT_NEAR(0.0, approximate_gradient({3.0})[0], 1e-6);
  EXPECT_NEAR(8.0, approximate_gradient({4.0})[0], 1e-6);  // == the derivative of `x^2` with `x = 4`.

  const fncas::g_intermediate intermediate_gradient(x, intermediate_function);
  EXPECT_EQ(0.0, intermediate_gradient({3.0})[0]);
  EXPECT_EQ(8.0, intermediate_gradient({4.0})[0]);

  fncas::g_compiled compiled_gradient(intermediate_function, intermediate_gradient);
  EXPECT_EQ(0.0, compiled_gradient({3.0})[0]) << compiled_gradient.lib_filename();
  EXPECT_EQ(8.0, compiled_gradient({4.0})[0]) << compiled_gradient.lib_filename();
}
