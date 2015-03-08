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
#include <string>

#include "mathutil.h"

namespace fncas {

struct OptimizationResult {
  double value;
  std::vector<double> point;
};

class OptimizerParameters {
 public:
  template <typename T>
  void SetValue(std::string name, T value);
  template <typename T>
  const T GetValue(std::string name, T default_value);

 private:
  std::map<std::string, double> params_;
};

template <typename T>
void OptimizerParameters::SetValue(std::string name, T value) {
  static_assert(std::is_arithmetic<T>::value, "Value must be numeric");
  params_[name] = value;
}

template <typename T>
const T OptimizerParameters::GetValue(std::string name, T default_value) {
  static_assert(std::is_arithmetic<T>::value, "Value must be numeric");
  if (params_.count(name)) {
    return static_cast<T>(params_[name]);
  } else {
    return default_value;
  }
}

// TODO(mzhurovich+dkorolev): Make different implementation to use functors.
// Naive gradient descent that tries 3 different step sizes in each iteration.
// Searches for a local minimum of `F::Compute` function.
template <class F>
class GradientDescentOptimizer : noncopyable {
 public:
  GradientDescentOptimizer() {}
  GradientDescentOptimizer(OptimizerParameters& params) {
    max_steps_ = params.GetValue("max_steps", max_steps_);
    step_factor_ = params.GetValue("step_factor", step_factor_);
  }
  OptimizationResult Optimize(const std::vector<double>& starting_point);

 private:
  size_t max_steps_ = 5000;   // Maximum number of optimization steps.
  double step_factor_ = 1.0;  // Gradient is multiplied by this factor.
};

// Simple gradient descent optimizer with backtracking algorithm.
// Searches for a local minimum of `F::Compute` function.
template <class F>
class GradientDescentOptimizerBT : noncopyable {
 public:
  GradientDescentOptimizerBT() {}
  GradientDescentOptimizerBT(OptimizerParameters& params) {
    min_steps_ = params.GetValue("min_steps", min_steps_);
    max_steps_ = params.GetValue("max_steps", max_steps_);
    bt_alpha_ = params.GetValue("bt_alpha", bt_alpha_);
    bt_beta_ = params.GetValue("bt_beta", bt_beta_);
    bt_max_steps_ = params.GetValue("bt_max_steps", bt_max_steps_);
    grad_eps_ = params.GetValue("grad_eps", grad_eps_);
  }
  OptimizationResult Optimize(const std::vector<double>& starting_point);

 private:
  size_t min_steps_ = 3;       // Minimum number of optimization steps (ignoring early stopping).
  size_t max_steps_ = 5000;    // Maximum number of optimization steps.
  double bt_alpha_ = 0.5;      // Alpha parameter for backtracking algorithm.
  double bt_beta_ = 0.8;       // Beta parameter for backtracking algorithm.
  size_t bt_max_steps_ = 100;  // Maximum number of backtracking steps.
  double grad_eps_ = 1e-8;     // Magnitude of gradient for early stopping.
};

// Optimizer that uses a combination of conjugate gradient method and
// backtracking line search to find a local minimum of `F::Compute` function.
template <class F>
class ConjugateGradientOptimizer : noncopyable {
 public:
  ConjugateGradientOptimizer() {}
  ConjugateGradientOptimizer(OptimizerParameters& params) {
    min_steps_ = params.GetValue("min_steps", min_steps_);
    max_steps_ = params.GetValue("max_steps", max_steps_);
    bt_alpha_ = params.GetValue("bt_alpha", bt_alpha_);
    bt_beta_ = params.GetValue("bt_beta", bt_beta_);
    bt_max_steps_ = params.GetValue("bt_max_steps", bt_max_steps_);
    grad_eps_ = params.GetValue("grad_eps", grad_eps_);
  }
  OptimizationResult Optimize(const std::vector<double>& starting_point);

 private:
  size_t min_steps_ = 3;       // Minimum number of optimization steps (ignoring early stopping).
  size_t max_steps_ = 5000;    // Maximum number of optimization steps.
  double bt_alpha_ = 0.5;      // Alpha parameter for backtracking algorithm.
  double bt_beta_ = 0.8;       // Beta parameter for backtracking algorithm.
  size_t bt_max_steps_ = 100;  // Maximum number of backtracking steps.
  double grad_eps_ = 1e-8;     // Magnitude of gradient for early stopping.
};

template <class F>
OptimizationResult GradientDescentOptimizer<F>::Optimize(const std::vector<double>& starting_point) {
  fncas::reset_internals_singleton();
  const size_t dim = starting_point.size();
  const fncas::x gradient_helper(dim);
  fncas::f_intermediate fi(F::compute(gradient_helper));
  fncas::g_intermediate gi = fncas::g_intermediate(gradient_helper, fi);
  std::vector<double> current_point(starting_point);

  for (size_t iteration = 0; iteration < max_steps_; ++iteration) {
    const auto g = gi(current_point);
    const auto try_step = [&dim, &current_point, &fi, &g ](double step) {
      const auto candidate_point(SumVectors(current_point, g.gradient, -step));
      return std::pair<double, std::vector<double>>(fi(candidate_point), candidate_point);
    };
    current_point = std::min(try_step(0.01 * step_factor_),
                             std::min(try_step(0.05 * step_factor_), try_step(0.2 * step_factor_))).second;
  }

  return OptimizationResult{fi(current_point), current_point};
}

template <class F>
OptimizationResult GradientDescentOptimizerBT<F>::Optimize(const std::vector<double>& starting_point) {
  fncas::reset_internals_singleton();
  const size_t dim = starting_point.size();
  const fncas::x gradient_helper(dim);
  fncas::f_intermediate fi(F::compute(gradient_helper));
  fncas::g_intermediate gi = fncas::g_intermediate(gradient_helper, fi);
  std::vector<double> current_point(starting_point);

  for (size_t iteration = 0; iteration < max_steps_; ++iteration) {
    auto direction = gi(current_point).gradient;
    fncas::FlipSign(direction);  // Going against the gradient to minimize the function.
    current_point = BackTracking(fi, gi, current_point, direction, bt_alpha_, bt_beta_, bt_max_steps_);

    // Simple early stopping by the norm of the gradient.
    if (std::sqrt(fncas::L2Norm(direction)) < grad_eps_ && iteration >= min_steps_) {
      break;
    }
  }

  return OptimizationResult{fi(current_point), current_point};
}

// TODO(mzhurovich): Implement more sophisticated version.
template <class F>
OptimizationResult ConjugateGradientOptimizer<F>::Optimize(const std::vector<double>& starting_point) {
  fncas::reset_internals_singleton();
  const size_t dim = starting_point.size();
  const fncas::x gradient_helper(dim);
  fncas::f_intermediate fi(F::compute(gradient_helper));
  fncas::g_intermediate gi = fncas::g_intermediate(gradient_helper, fi);
  std::vector<double> current_point(starting_point);

  const auto initial_f_gradf = gi(current_point);
  std::vector<double> current_grad = initial_f_gradf.gradient;
  std::vector<double> s(current_grad);  // Direction to search for a minimum.
  fncas::FlipSign(s);                   // Trying first step against the gradient to minimize the function.

  for (size_t iteration = 0; iteration < max_steps_; ++iteration) {
    // Backtracking line search.
    const auto new_point = fncas::BackTracking(fi, gi, current_point, s, bt_alpha_, bt_beta_, bt_max_steps_);
    const auto new_f_gradf = gi(new_point);

    // Calculating direction for the next step.
    const double omega = std::max(fncas::PolakRibiere(new_f_gradf.gradient, current_grad), 0.0);
    s = SumVectors(s, new_f_gradf.gradient, omega, -1.0);

    current_grad = new_f_gradf.gradient;
    current_point = new_point;

    // Simple early stopping by the norm of the gradient.
    if (std::sqrt(L2Norm(s)) < grad_eps_ && iteration >= min_steps_) {
      break;
    }
  }

  return OptimizationResult{fi(current_point), current_point};
}

}  // namespace fncas

#endif  // #ifndef FNCAS_OPTIMIZE_H
