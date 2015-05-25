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

#ifndef FNCAS_MATHUTIL_H
#define FNCAS_MATHUTIL_H

#include <cassert>
#include <cmath>
#include <functional>
#include <numeric>
#include <vector>

namespace fncas {

inline bool IsNormal(double arg) { return (std::isnormal(arg) || arg == 0.0); }

template <typename T>
inline typename std::enable_if<std::is_arithmetic<T>::value, std::vector<T>>::type SumVectors(
    std::vector<T> a, const std::vector<T>& b) {
#ifndef NDEBUG
  assert(a.size() == b.size());
#endif
  for (size_t i = 0; i < a.size(); ++i) {
    a[i] += b[i];
  }
  return std::move(a);
}

template <typename T>
inline typename std::enable_if<std::is_arithmetic<T>::value, std::vector<T>>::type SumVectors(
    std::vector<T> a, const std::vector<T>& b, double kb) {
#ifndef NDEBUG
  assert(a.size() == b.size());
#endif
  for (size_t i = 0; i < a.size(); ++i) {
    a[i] += kb * b[i];
  }
  return std::move(a);
}

template <typename T>
inline typename std::enable_if<std::is_arithmetic<T>::value, std::vector<T>>::type SumVectors(
    std::vector<T> a, const std::vector<T>& b, double ka, double kb) {
#ifndef NDEBUG
  assert(a.size() == b.size());
#endif
  for (size_t i = 0; i < a.size(); ++i) {
    a[i] = ka * a[i] + kb * b[i];
  }
  return std::move(a);
}

template <typename T>
inline typename std::enable_if<std::is_arithmetic<T>::value, T>::type DotProduct(const std::vector<T>& v1,
                                                                                 const std::vector<T>& v2) {
#ifndef NDEBUG
  assert(v1.size() == v2.size());
#endif
  return std::inner_product(std::begin(v1), std::end(v1), std::begin(v2), static_cast<T>(0));
}

template <typename T>
inline typename std::enable_if<std::is_arithmetic<T>::value, T>::type L2Norm(const std::vector<T>& v) {
  return DotProduct(v, v);
}

template <typename T>
inline typename std::enable_if<std::is_arithmetic<T>::value>::type FlipSign(std::vector<T>& v) {
  std::transform(std::begin(v), std::end(v), std::begin(v), std::negate<T>());
}

// Polak-Ribiere formula for conjugate gradient method
// http://en.wikipedia.org/wiki/Nonlinear_conjugate_gradient_method
inline double PolakRibiere(const std::vector<double>& g, const std::vector<double>& g_prev) {
  const double beta = (L2Norm(g) - DotProduct(g, g_prev)) / L2Norm(g_prev);
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
template <class F, class G>
inline std::vector<double> BackTracking(F&& eval_function,
                                        G&& eval_gradient,
                                        const std::vector<double>& current_point,
                                        const std::vector<double>& direction,
                                        const double alpha = 0.5,
                                        const double beta = 0.8,
                                        const size_t max_steps = 100) {
  const double current_f_value = eval_function(current_point);
  const std::vector<double> current_gradient = eval_gradient(current_point);
  std::vector<double> test_point = SumVectors(current_point, direction);
  double test_f_value = eval_function(test_point);
  const double gradient_l2norm = L2Norm(current_gradient);
  double t = 1.0;

  size_t bt_iteration = 0;
  while (test_f_value > (current_f_value - alpha * t * gradient_l2norm) || !IsNormal(test_f_value)) {
    t *= beta;
    test_point = SumVectors(current_point, direction, t);
    test_f_value = eval_function(test_point);
    if (bt_iteration++ == max_steps) {
      break;
    }
  }

  return test_point;
}

}  // namespace fncas

#endif  // #ifndef FNCAS_MATHUTIL_H
