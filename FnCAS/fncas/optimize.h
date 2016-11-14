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
#include <map>
#include <string>

#include "base.h"
#include "differentiate.h"
#include "node.h"
#include "mathutil.h"

#include "../../Bricks/template/decay.h"

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
  const T GetValue(std::string name, T default_value) const;

 private:
  std::map<std::string, double> params_;
};

template <typename T>
void OptimizerParameters::SetValue(std::string name, T value) {
  static_assert(std::is_arithmetic<T>::value, "Value must be numeric");
  params_[name] = value;
}

template <typename T>
const T OptimizerParameters::GetValue(std::string name, T default_value) const {
  static_assert(std::is_arithmetic<T>::value, "Value must be numeric");
  if (params_.count(name)) {
    return static_cast<T>(params_.at(name));
  } else {
    return default_value;
  }
}

// TODO(mzhurovich+dkorolev): Make different implementation to use functors.
// Naive gradient descent that tries 3 different step sizes in each iteration.
// Searches for a local minimum of `F::ObjectiveFunction` function.
template <class F>
class GradientDescentOptimizer : noncopyable {
 public:
  GradientDescentOptimizer() : f_instance_(std::make_unique<F>()), f_reference_(*f_instance_) {}
  GradientDescentOptimizer(const OptimizerParameters& params)
      : f_instance_(std::make_unique<F>()), f_reference_(*f_instance_) {
    InitializeParams(params);
  }

  GradientDescentOptimizer(F& f) : f_reference_(f) {}
  GradientDescentOptimizer(const OptimizerParameters& params, F& f) : f_reference_(f) { InitializeParams(params); }

  template <typename ARG,
            class = std::enable_if_t<!std::is_same<current::decay<ARG>, OptimizerParameters>::value>,
            typename... ARGS>
  GradientDescentOptimizer(ARG&& arg, ARGS&&... args)
      : f_instance_(std::make_unique<F>(std::forward<ARG>(arg), std::forward<ARGS>(args)...)),
        f_reference_(*f_instance_) {}

  template <typename ARG, typename... ARGS>
  GradientDescentOptimizer(const OptimizerParameters& params, ARG&& arg, ARGS&&... args)
      : f_instance_(std::make_unique<F>(std::forward<ARG>(arg), std::forward<ARGS>(args)...)),
        f_reference_(*f_instance_) {
    InitializeParams(params);
  }

  void InitializeParams(const OptimizerParameters& params) {
    max_steps_ = params.GetValue("max_steps", max_steps_);
    step_factor_ = params.GetValue("step_factor", step_factor_);
  }

  F* operator->() { return &f_reference_; }

  OptimizationResult Optimize(const std::vector<double>& starting_point) {
    const size_t dim = starting_point.size();
    const fncas::X gradient_helper(dim);
    const fncas::f_intermediate fi(f_reference_.ObjectiveFunction(gradient_helper));
    const fncas::g_intermediate gi(gradient_helper, fi);
    std::vector<double> current_point(starting_point);

    for (size_t iteration = 0; iteration < max_steps_; ++iteration) {
      const auto g = gi(current_point);
      const auto try_step = [&dim, &current_point, &fi, &g](double step) {
        const auto candidate_point(SumVectors(current_point, g, -step));
        return std::make_pair(fi(candidate_point), candidate_point);
      };
      current_point = std::min(try_step(0.01 * step_factor_),
                               std::min(try_step(0.05 * step_factor_), try_step(0.2 * step_factor_))).second;
    }

    return OptimizationResult{fi(current_point), current_point};
  }

 private:
  std::unique_ptr<F> f_instance_;  // The function to optimize: instance of owned by the optimizer.
  F& f_reference_;                 // The function to optimize: reference to work with, owned or not owned.

  size_t max_steps_ = 5000;   // Maximum number of optimization steps.
  double step_factor_ = 1.0;  // Gradient is multiplied by this factor.
};

// Simple gradient descent optimizer with backtracking algorithm.
// Searches for a local minimum of `F::ObjectiveFunction` function.
template <class F>
class GradientDescentOptimizerBT : noncopyable {
 public:
  GradientDescentOptimizerBT() : f_instance_(std::make_unique<F>()), f_reference_(*f_instance_) {}
  GradientDescentOptimizerBT(const OptimizerParameters& params)
      : f_instance_(std::make_unique<F>()), f_reference_(*f_instance_) {
    InitializeParams(params);
  }

  GradientDescentOptimizerBT(F& f) : f_reference_(f) {}
  GradientDescentOptimizerBT(const OptimizerParameters& params, F& f) : f_reference_(f) { InitializeParams(params); }

  template <typename ARG,
            class = std::enable_if_t<!std::is_same<current::decay<ARG>, OptimizerParameters>::value>,
            typename... ARGS>
  GradientDescentOptimizerBT(ARG&& arg, ARGS&&... args)
      : f_instance_(std::make_unique<F>(std::forward<ARG>(arg), std::forward<ARGS>(args)...)),
        f_reference_(*f_instance_) {}

  template <typename ARG, typename... ARGS>
  GradientDescentOptimizerBT(const OptimizerParameters& params, ARG&& arg, ARGS&&... args)
      : f_instance_(std::make_unique<F>(std::forward<ARG>(arg), std::forward<ARGS>(args)...)),
        f_reference_(*f_instance_) {
    InitializeParams(params);
  }

  void InitializeParams(const OptimizerParameters& params) {
    min_steps_ = params.GetValue("min_steps", min_steps_);
    max_steps_ = params.GetValue("max_steps", max_steps_);
    bt_alpha_ = params.GetValue("bt_alpha", bt_alpha_);
    bt_beta_ = params.GetValue("bt_beta", bt_beta_);
    bt_max_steps_ = params.GetValue("bt_max_steps", bt_max_steps_);
    grad_eps_ = params.GetValue("grad_eps", grad_eps_);
  }

  F* operator->() { return &f_reference_; }

  OptimizationResult Optimize(const std::vector<double>& starting_point) {
    const size_t dim = starting_point.size();
    const fncas::X gradient_helper(dim);
    const fncas::f_intermediate fi(f_reference_.ObjectiveFunction(gradient_helper));
    const fncas::g_intermediate gi(gradient_helper, fi);
    std::vector<double> current_point(starting_point);

    for (size_t iteration = 0; iteration < max_steps_; ++iteration) {
      auto direction = gi(current_point);
      fncas::FlipSign(direction);  // Going against the gradient to minimize the function.
      current_point = Backtracking(fi, gi, current_point, direction, bt_alpha_, bt_beta_, bt_max_steps_);

      // Simple early stopping by the norm of the gradient.
      if (std::sqrt(fncas::L2Norm(direction)) < grad_eps_ && iteration >= min_steps_) {
        break;
      }
    }

    return OptimizationResult{fi(current_point), current_point};
  }

 private:
  std::unique_ptr<F> f_instance_;  // The function to optimize: instance of owned by the optimizer.
  F& f_reference_;                 // The function to optimize: reference to work with, owned or not owned.

  size_t min_steps_ = 3;       // Minimum number of optimization steps (ignoring early stopping).
  size_t max_steps_ = 5000;    // Maximum number of optimization steps.
  double bt_alpha_ = 0.5;      // Alpha parameter for backtracking algorithm.
  double bt_beta_ = 0.8;       // Beta parameter for backtracking algorithm.
  size_t bt_max_steps_ = 100;  // Maximum number of backtracking steps.
  double grad_eps_ = 1e-8;     // Magnitude of gradient for early stopping.
};

// Optimizer that uses a combination of conjugate gradient method and
// backtracking line search to find a local minimum of `F::ObjectiveFunction` function.
template <class F>
class ConjugateGradientOptimizer : noncopyable {
 public:
  ConjugateGradientOptimizer() : f_instance_(std::make_unique<F>()), f_reference_(*f_instance_) {}
  ConjugateGradientOptimizer(const OptimizerParameters& params)
      : f_instance_(std::make_unique<F>()), f_reference_(*f_instance_) {
    InitializeParams(params);
  }

  ConjugateGradientOptimizer(F& f) : f_reference_(f) {}
  ConjugateGradientOptimizer(const OptimizerParameters& params, F& f) : f_reference_(f) { InitializeParams(params); }

  template <typename ARG,
            class = std::enable_if_t<!std::is_same<current::decay<ARG>, OptimizerParameters>::value>,
            typename... ARGS>
  ConjugateGradientOptimizer(ARG&& arg, ARGS&&... args)
      : f_instance_(std::make_unique<F>(std::forward<ARG>(arg), std::forward<ARGS>(args)...)),
        f_reference_(*f_instance_) {}

  template <typename ARG, typename... ARGS>
  ConjugateGradientOptimizer(const OptimizerParameters& params, ARG&& arg, ARGS&&... args)
      : f_instance_(std::make_unique<F>(std::forward<ARG>(arg), std::forward<ARGS>(args)...)),
        f_reference_(*f_instance_) {
    InitializeParams(params);
  }

  void InitializeParams(const OptimizerParameters& params) {
    min_steps_ = params.GetValue("min_steps", min_steps_);
    max_steps_ = params.GetValue("max_steps", max_steps_);
    bt_alpha_ = params.GetValue("bt_alpha", bt_alpha_);
    bt_beta_ = params.GetValue("bt_beta", bt_beta_);
    bt_max_steps_ = params.GetValue("bt_max_steps", bt_max_steps_);
    grad_eps_ = params.GetValue("grad_eps", grad_eps_);
  }
  F* operator->() { return &f_reference_; }

  OptimizationResult Optimize(const std::vector<double>& starting_point) {
    // TODO(mzhurovich): Implement a more sophisticated version.
    const size_t dim = starting_point.size();
    const fncas::X gradient_helper(dim);
    const fncas::f_intermediate fi(f_reference_.ObjectiveFunction(gradient_helper));
    const fncas::g_intermediate gi(gradient_helper, fi);
    std::vector<double> current_point(starting_point);

    std::vector<double> current_gradient = gi(current_point);
    std::vector<double> s(current_gradient);  // Direction to search for a minimum.
    fncas::FlipSign(s);                       // Trying first step against the gradient to minimize the function.

    for (size_t iteration = 0; iteration < max_steps_; ++iteration) {
      // Backtracking line search.
      const auto new_point = fncas::Backtracking(fi, gi, current_point, s, bt_alpha_, bt_beta_, bt_max_steps_);
      const auto new_gradient = gi(new_point);

      // Calculating direction for the next step.
      const double omega = std::max(fncas::PolakRibiere(new_gradient, current_gradient), 0.0);
      s = SumVectors(s, new_gradient, omega, -1.0);

      current_gradient = new_gradient;
      current_point = new_point;

      // Simple early stopping by the norm of the gradient.
      if (std::sqrt(L2Norm(s)) < grad_eps_ && iteration >= min_steps_) {
        break;
      }
    }

    return OptimizationResult{fi(current_point), current_point};
  }

 private:
  std::unique_ptr<F> f_instance_;  // The function to optimize: instance of owned by the optimizer.
  F& f_reference_;                 // The function to optimize: reference to work with, owned or not owned.

  size_t min_steps_ = 3;       // Minimum number of optimization steps (ignoring early stopping).
  size_t max_steps_ = 5000;    // Maximum number of optimization steps.
  double bt_alpha_ = 0.5;      // Alpha parameter for backtracking algorithm.
  double bt_beta_ = 0.8;       // Beta parameter for backtracking algorithm.
  size_t bt_max_steps_ = 100;  // Maximum number of backtracking steps.
  double grad_eps_ = 1e-8;     // Magnitude of gradient for early stopping.
};

}  // namespace fncas

#endif  // #ifndef FNCAS_OPTIMIZE_H
