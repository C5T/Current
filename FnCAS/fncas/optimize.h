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
#include "../../TypeSystem/struct.h"
#include "../../TypeSystem/optional.h"
#include "../../TypeSystem/helpers.h"

namespace fncas {

CURRENT_STRUCT(OptimizationResult) {
  CURRENT_FIELD(value, double, 0.0);
  CURRENT_FIELD(point, std::vector<double>);
  CURRENT_CONSTRUCTOR(OptimizationResult)(double value, const std::vector<double>& x) : value(value), point(x) {}
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

template <class F>
class Optimizer : noncopyable {
 public:
  virtual ~Optimizer() = default;

  Optimizer() : f_instance_(std::make_unique<F>()), f_reference_(*f_instance_) {}
  Optimizer(const OptimizerParameters& parameters)
      : f_instance_(std::make_unique<F>()), f_reference_(*f_instance_), parameters_(parameters) {}

  Optimizer(F& f) : f_reference_(f) {}
  Optimizer(const OptimizerParameters& parameters, F& f) : f_reference_(f), parameters_(parameters) {}

  template <typename ARG,
            class = std::enable_if_t<!std::is_same<current::decay<ARG>, OptimizerParameters>::value>,
            typename... ARGS>
  Optimizer(ARG&& arg, ARGS&&... args)
      : f_instance_(std::make_unique<F>(std::forward<ARG>(arg), std::forward<ARGS>(args)...)),
        f_reference_(*f_instance_) {}

  template <typename ARG, typename... ARGS>
  Optimizer(const OptimizerParameters& parameters, ARG&& arg, ARGS&&... args)
      : f_instance_(std::make_unique<F>(std::forward<ARG>(arg), std::forward<ARGS>(args)...)),
        f_reference_(*f_instance_),
        parameters_(parameters) {}

  F& Function() { return f_reference_; }
  const F& Function() const { return f_reference_; }
  F* operator->() { return &f_reference_; }
  const F* operator->() const { return &f_reference_; }

  const Optional<OptimizerParameters> Parameters() const { return parameters_; }

  virtual OptimizationResult Optimize(const std::vector<double>& starting_point) const = 0;

 private:
  std::unique_ptr<F> f_instance_;             // The function to optimize: instance if owned by the optimizer.
  F& f_reference_;                            // The function to optimize: reference to work with, owned or not owned.
  Optional<OptimizerParameters> parameters_;  // Optimization parameters.
};

// Naive gradient descent that tries 3 different step sizes in each iteration.
// Searches for a local minimum of `F::ObjectiveFunction` function.
template <class F>
class GradientDescentOptimizer : public Optimizer<F> {
 public:
  using super_t = Optimizer<F>;
  using super_t::super_t;

  OptimizationResult Optimize(const std::vector<double>& starting_point) const override {
    size_t max_steps = 5000;   // Maximum number of optimization steps.
    double step_factor = 1.0;  // Gradient is multiplied by this factor.

    if (Exists(super_t::Parameters())) {
      const auto& parameters = Value(super_t::Parameters());
      max_steps = parameters.GetValue("max_steps", max_steps);
      step_factor = parameters.GetValue("step_factor", step_factor);
    }

    const size_t dim = starting_point.size();
    const fncas::X gradient_helper(dim);
    const fncas::f_intermediate fi(super_t::Function().ObjectiveFunction(gradient_helper));
    const fncas::g_intermediate gi(gradient_helper, fi);
    std::vector<double> current_point(starting_point);

    for (size_t iteration = 0; iteration < max_steps; ++iteration) {
      const auto g = gi(current_point);
      const auto try_step = [&dim, &current_point, &fi, &g](double step) {
        const auto candidate_point(SumVectors(current_point, g, -step));
        return std::make_pair(fi(candidate_point), candidate_point);
      };
      current_point = std::min(try_step(0.01 * step_factor),
                               std::min(try_step(0.05 * step_factor), try_step(0.2 * step_factor))).second;
    }

    return OptimizationResult(fi(current_point), current_point);
  }
};

// Simple gradient descent optimizer with backtracking algorithm.
// Searches for a local minimum of `F::ObjectiveFunction` function.
template <class F>
class GradientDescentOptimizerBT : public Optimizer<F> {
 public:
  using super_t = Optimizer<F>;
  using super_t::super_t;

  OptimizationResult Optimize(const std::vector<double>& starting_point) const override {
    size_t min_steps = 3;       // Minimum number of optimization steps (ignoring early stopping).
    size_t max_steps = 5000;    // Maximum number of optimization steps.
    double bt_alpha = 0.5;      // Alpha parameter for backtracking algorithm.
    double bt_beta = 0.8;       // Beta parameter for backtracking algorithm.
    size_t bt_max_steps = 100;  // Maximum number of backtracking steps.
    double grad_eps = 1e-8;     // Magnitude of gradient for early stopping.

    if (Exists(super_t::Parameters())) {
      const auto& parameters = Value(super_t::Parameters());
      min_steps = parameters.GetValue("min_steps", min_steps);
      max_steps = parameters.GetValue("max_steps", max_steps);
      bt_alpha = parameters.GetValue("bt_alpha", bt_alpha);
      bt_beta = parameters.GetValue("bt_beta", bt_beta);
      bt_max_steps = parameters.GetValue("bt_max_steps", bt_max_steps);
      grad_eps = parameters.GetValue("grad_eps", grad_eps);
    }

    const size_t dim = starting_point.size();
    const fncas::X gradient_helper(dim);
    const fncas::f_intermediate fi(super_t::Function().ObjectiveFunction(gradient_helper));
    const fncas::g_intermediate gi(gradient_helper, fi);
    std::vector<double> current_point(starting_point);

    for (size_t iteration = 0; iteration < max_steps; ++iteration) {
      auto direction = gi(current_point);
      fncas::FlipSign(direction);  // Going against the gradient to minimize the function.
      current_point = Backtracking(fi, gi, current_point, direction, bt_alpha, bt_beta, bt_max_steps);

      // Simple early stopping by the norm of the gradient.
      if (std::sqrt(fncas::L2Norm(direction)) < grad_eps && iteration >= min_steps) {
        break;
      }
    }

    return OptimizationResult(fi(current_point), current_point);
  }
};

// Optimizer that uses a combination of conjugate gradient method and
// backtracking line search to find a local minimum of `F::ObjectiveFunction` function.
template <class F>
class ConjugateGradientOptimizer : public Optimizer<F> {
 public:
  using super_t = Optimizer<F>;
  using super_t::super_t;

  OptimizationResult Optimize(const std::vector<double>& starting_point) const override {
    size_t min_steps = 3;       // Minimum number of optimization steps (ignoring early stopping).
    size_t max_steps = 5000;    // Maximum number of optimization steps.
    double bt_alpha = 0.5;      // Alpha parameter for backtracking algorithm.
    double bt_beta = 0.8;       // Beta parameter for backtracking algorithm.
    size_t bt_max_steps = 100;  // Maximum number of backtracking steps.
    double grad_eps = 1e-8;     // Magnitude of gradient for early stopping.

    if (Exists(super_t::Parameters())) {
      const auto& parameters = Value(super_t::Parameters());
      min_steps = parameters.GetValue("min_steps", min_steps);
      max_steps = parameters.GetValue("max_steps", max_steps);
      bt_alpha = parameters.GetValue("bt_alpha", bt_alpha);
      bt_beta = parameters.GetValue("bt_beta", bt_beta);
      bt_max_steps = parameters.GetValue("bt_max_steps", bt_max_steps);
      grad_eps = parameters.GetValue("grad_eps", grad_eps);
    }

    // TODO(mzhurovich): Implement a more sophisticated version.
    const size_t dim = starting_point.size();
    const fncas::X gradient_helper(dim);
    const fncas::f_intermediate fi(super_t::Function().ObjectiveFunction(gradient_helper));
    const fncas::g_intermediate gi(gradient_helper, fi);
    std::vector<double> current_point(starting_point);

    std::vector<double> current_gradient = gi(current_point);
    std::vector<double> s(current_gradient);  // Direction to search for a minimum.
    fncas::FlipSign(s);                       // Trying first step against the gradient to minimize the function.

    for (size_t iteration = 0; iteration < max_steps; ++iteration) {
      // Backtracking line search.
      const auto new_point = fncas::Backtracking(fi, gi, current_point, s, bt_alpha, bt_beta, bt_max_steps);
      const auto new_gradient = gi(new_point);

      // Calculating direction for the next step.
      const double omega = std::max(fncas::PolakRibiere(new_gradient, current_gradient), 0.0);
      s = SumVectors(s, new_gradient, omega, -1.0);

      current_gradient = new_gradient;
      current_point = new_point;

      // Simple early stopping by the norm of the gradient.
      if (std::sqrt(L2Norm(s)) < grad_eps && iteration >= min_steps) {
        break;
      }
    }

    return OptimizationResult{fi(current_point), current_point};
  }
};

}  // namespace fncas

#endif  // #ifndef FNCAS_OPTIMIZE_H
