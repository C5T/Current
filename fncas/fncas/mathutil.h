/*******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * *******************************************************************************/

#ifndef FNCAS_FNCAS_MATHUTIL_H
#define FNCAS_FNCAS_MATHUTIL_H

#include "base.h"
#include "exceptions.h"
#include "logger.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>
#include <vector>

#include "../../typesystem/struct.h"
#include "../../typesystem/serialization/json.h"

namespace fncas {

CURRENT_STRUCT(ValueAndPoint) {
  CURRENT_FIELD(value, double_t, 0.0);
  CURRENT_FIELD(point, std::vector<double_t>);
  bool operator<(const ValueAndPoint& rhs) const { return value < rhs.value; }
  CURRENT_CONSTRUCTOR(ValueAndPoint)(double_t value, const std::vector<double_t>& x) : value(value), point(x) {}
};

inline bool IsNormal(double_t arg) { return (std::isnormal(arg) || arg == 0.0); }

namespace impl {

template <typename T>
std::enable_if_t<std::is_arithmetic_v<T>, std::vector<T>> SumVectors(std::vector<T> a,
                                                                     const std::vector<T>& b) {
#ifndef NDEBUG
  CURRENT_ASSERT(a.size() == b.size());
#endif
  for (size_t i = 0; i < a.size(); ++i) {
    a[i] += b[i];
    if (std::abs(a[i]) < 1e-100) {
      // NOTE(dkorolev): Fight the underflow.
      a[i] = 0.0;
    }
  }
  return a;
}

template <typename T>
std::enable_if_t<std::is_arithmetic_v<T>, std::vector<T>>SumVectors(std::vector<T> a,
                                                                    const std::vector<T>& b,
                                                                    double_t kb) {
#ifndef NDEBUG
  CURRENT_ASSERT(a.size() == b.size());
#endif
  for (size_t i = 0; i < a.size(); ++i) {
    a[i] += kb * b[i];
    if (std::abs(a[i]) < 1e-100) {
      // NOTE(dkorolev): Fight the underflow.
      a[i] = 0.0;
    }
  }
  return a;
}

template <typename T>
std::enable_if_t<std::is_arithmetic_v<T>, std::vector<T>> SumVectors(std::vector<T> a,
                                                                     const std::vector<T>& b,
                                                                     double_t ka,
                                                                     double_t kb) {
#ifndef NDEBUG
  CURRENT_ASSERT(a.size() == b.size());
#endif
  for (size_t i = 0; i < a.size(); ++i) {
    a[i] = ka * a[i] + kb * b[i];
    if (std::abs(a[i]) < 1e-100) {
      // NOTE(dkorolev): Fight the underflow.
      a[i] = 0.0;
    }
  }
  return a;
}

template <typename T>
std::enable_if_t<std::is_arithmetic_v<T>, T> DotProduct(const std::vector<T>& v1,
                                                        const std::vector<T>& v2) {
#ifndef NDEBUG
  CURRENT_ASSERT(v1.size() == v2.size());
#endif
  return std::inner_product(std::begin(v1), std::end(v1), std::begin(v2), static_cast<T>(0));
}

template <typename T>
std::enable_if_t<std::is_arithmetic_v<T>, T> L2Norm(const std::vector<T>& v) {
  return DotProduct(v, v);
}

template <typename T>
std::enable_if_t<std::is_arithmetic_v<T>> FlipSign(std::vector<T>& v) {
  std::transform(std::begin(v), std::end(v), std::begin(v), std::negate<T>());
}

// Polak-Ribiere formula for conjugate gradient method
// http://en.wikipedia.org/wiki/Nonlinear_conjugate_gradient_method
inline double_t PolakRibiere(const std::vector<double_t>& g, const std::vector<double_t>& g_prev) {
  const double_t beta = (L2Norm(g) - DotProduct(g, g_prev)) / L2Norm(g_prev);
  if (IsNormal(beta)) {
    return beta;
  } else {
    return 0.0;
  }
}

// Simplified backtracking line search algorithm with limited number of steps.
// Starts in `current_point` and searches for minimum in `direction`
// sequentially shrinking the step size. Returns new optimal point.
// Algorithm parameters: 0 < alpha < 1, 0 < beta < 1.
template <OptimizationDirection DIRECTION, class F, class G>
ValueAndPoint Backtracking(F&& f,
                           G&& g,
                           const std::vector<double_t>& current_point,
                           const std::vector<double_t>& direction,
                           optimize::OptimizerStats& stats,
                           const double_t alpha = 0.5,
                           const double_t beta = 0.8,
                           const size_t max_steps = 100) {
  const double sign = (DIRECTION == OptimizationDirection::Minimize ? +1 : -1);
  const auto& logger = OptimizerLogger();
  stats.JournalBacktrackingCall();
  stats.JournalFunction();
  const double_t current_f_value = f(current_point) * sign;
  std::vector<double_t> test_point = SumVectors(current_point, direction);
  stats.JournalFunction();
  double_t test_f_value = f(test_point) * sign;
  if (logger) {
    logger.Log(std::string("Backtracking: Starting value to minimize, OK if NAN or INF, is ") +
               current::ToString(test_f_value));
  }
  stats.JournalGradient();
  std::vector<double_t> current_gradient = g(current_point);
  if (DIRECTION == OptimizationDirection::Maximize) {
    FlipSign(current_gradient);
  }
  const double_t gradient_l2norm = L2Norm(current_gradient);
  double_t t = 1.0;

  size_t bt_iteration = 0;
  while (test_f_value > (current_f_value - alpha * t * gradient_l2norm) || !IsNormal(test_f_value)) {
    stats.JournalBacktrackingStep();
    t *= beta;
    test_point = SumVectors(current_point, direction, t);
    stats.JournalFunction();
    test_f_value = f(test_point) * sign;
    if (bt_iteration++ == max_steps) {
      break;
    }
  }

  if (IsNormal(test_f_value)) {
    if (logger) {
      logger.Log(std::string("Backtracking: Final value to minimize = ") + current::ToString(test_f_value));
    }
    return ValueAndPoint(test_f_value, test_point);
  } else {
    if (logger) {
      logger.Log("Backtracking: No non-NAN and non-INF step found.");
    }
    CURRENT_THROW(exceptions::BacktrackingException("!fncas::impl::IsNormal(test_f_value)"));
  }
}

}  // namespace fncas::impl
}  // namespace fncas

#endif  // #ifndef FNCAS_FNCAS_MATHUTIL_H
