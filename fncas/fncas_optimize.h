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

#ifndef FNCAS_OPTIMIZE_H
#define FNCAS_OPTIMIZE_H

#include <algorithm>
#include <numeric>

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
template<class F> OptimizationResult OptimizeUsingGradientDescent(F&& user_f,
    const std::vector<double>& starting_point) { 
  const double ALPHA = 0.5;
  const double BETA = 0.8;
  const double GRADIENT_EPS = 1e-8;
  const int MAX_ITERATION_COUNT = 5000;

  fncas::reset_internals_singleton();
  const size_t dim = starting_point.size();
  const fncas::x gradient_helper(dim);
  fncas::f_intermediate fi(user_f(gradient_helper));
  fncas::g_intermediate gi = fncas::g_intermediate(gradient_helper, fi);
  std::vector<double> current_point(starting_point);

  for (size_t iteration = 0; iteration < MAX_ITERATION_COUNT; ++iteration) {
    const auto g = gi(current_point);
    const double current_f_value = fi(current_point);
    double squared_gradient = 0.0;
    std::vector<double> test_point(dim);

    for (size_t i = 0; i < dim; ++i) {
      test_point[i] = current_point[i] - g.gradient[i];
      squared_gradient += g.gradient[i] * g.gradient[i];
    }

    double t = 1.0;
    double test_f_value = fi(test_point);

    while (test_f_value > (current_f_value - ALPHA * t * squared_gradient)) {
      t *= BETA;
      for (size_t i = 0; i < dim; ++i) {
        test_point[i] = current_point[i] - t * g.gradient[i];
      }
      test_f_value = fi(test_point);
    }
    for (size_t i = 0; i < dim; ++i) {
      current_point[i] -= t * g.gradient[i];
    }
   if (std::sqrt(squared_gradient) < GRADIENT_EPS) {
      break;
    }
  }

 return OptimizationResult{fi(current_point), current_point};
}

#endif
