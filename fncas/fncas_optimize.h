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

#include "fncas_mathutil.h"

namespace fncas {

struct OptimizationResult {
  double value;
  std::vector<double> point;
};

class OptimizerParameters {
public:
  template<typename T> void SetValue(std::string name, T value);
  template<typename T> const T GetValue(std::string name, T default_value);
private:
  std::map<std::string, double> params_;
};

template<typename T> 
void OptimizerParameters::SetValue(std::string name, T value) { 
  static_assert(std::is_arithmetic<T>::value, "Value must be numeric");
  params_[name] = value; 
}

template<typename T> 
const T OptimizerParameters::GetValue(std::string name, T default_value) {
  static_assert(std::is_arithmetic<T>::value, "Value must be numeric");
  if (params_.count(name)) {
    return static_cast<T>(params_[name]);
  } else {
    return default_value;
  }
}

// TODO(mzhurovich+dkorolev): Make different implementation to use functors.
template <class F>
class GradientDescentOptimizer : noncopyable {
public:
  GradientDescentOptimizer() { }
  GradientDescentOptimizer(OptimizerParameters &params) {
    max_steps_ = params.GetValue("max_steps", max_steps_);
  }
  OptimizationResult Optimize(const std::vector<double>& starting_point);
private:
  size_t max_steps_ = 5000;
};

template <class F>
class GradientDescentOptimizerBT : noncopyable {
public:
  GradientDescentOptimizerBT() { }
  GradientDescentOptimizerBT(OptimizerParameters &params) {
    max_steps_ = params.GetValue("max_steps", max_steps_);
    bt_alpha_ = params.GetValue("bt_alpha", bt_alpha_);
    bt_beta_ = params.GetValue("bt_beta", bt_beta_);
    bt_max_steps_ = params.GetValue("bt_max_steps", bt_max_steps_);
    grad_eps_ = params.GetValue("grad_eps", grad_eps_);
  }
  OptimizationResult Optimize(const std::vector<double>& starting_point);
private:
  size_t max_steps_ = 5000;
  double bt_alpha_ = 0.5;
  double bt_beta_ = 0.8;
  size_t bt_max_steps_ = 100;
  double grad_eps_ = 1e-8;
};

template <class F>
class ConjugateGradientOptimizer : noncopyable {
public:
  ConjugateGradientOptimizer() { }
  ConjugateGradientOptimizer(OptimizerParameters &params) {
    max_steps_ = params.GetValue("max_steps", max_steps_);
    bt_alpha_ = params.GetValue("bt_alpha", bt_alpha_);
    bt_beta_ = params.GetValue("bt_beta", bt_beta_);
    bt_max_steps_ = params.GetValue("bt_max_steps", bt_max_steps_);
    grad_eps_ = params.GetValue("grad_eps", grad_eps_);
  }
  OptimizationResult Optimize(const std::vector<double>& starting_point);
private:
  size_t max_steps_ = 5000;
  double bt_alpha_ = 0.5;
  double bt_beta_ = 0.8;
  size_t bt_max_steps_ = 100;
  double grad_eps_ = 1e-8;

  double PolakRibiere(const std::vector<double> &g, const std::vector<double> &g_prev);
};

template<class F> 
OptimizationResult GradientDescentOptimizer<F>::Optimize(const std::vector<double>& starting_point) {
  fncas::reset_internals_singleton();
  const size_t dim = starting_point.size();
  const fncas::x gradient_helper(dim);
  fncas::f_intermediate fi(F::compute(gradient_helper));
  fncas::g_intermediate gi = fncas::g_intermediate(gradient_helper, fi);
  std::vector<double> current_point(starting_point);

  for (size_t iteration = 0; iteration < max_steps_; ++iteration) {
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


template<class F> 
OptimizationResult GradientDescentOptimizerBT<F>::Optimize(const std::vector<double>& starting_point) {
  fncas::reset_internals_singleton();
  const size_t dim = starting_point.size();
  const fncas::x gradient_helper(dim);
  fncas::f_intermediate fi(F::compute(gradient_helper));
  fncas::g_intermediate gi = fncas::g_intermediate(gradient_helper, fi);
  std::vector<double> current_point(starting_point);

  for (size_t iteration = 0; iteration < max_steps_; ++iteration) {
    const auto current_f_gradf = gi(current_point);
    const double current_f_value = current_f_gradf.value;
    double squared_gradient = 0.0;
    std::vector<double> test_point(dim);

    for (size_t i = 0; i < dim; ++i) {
      test_point[i] = current_point[i] - current_f_gradf.gradient[i];
      squared_gradient += current_f_gradf.gradient[i] * current_f_gradf.gradient[i];
    }

    double t = 1.0;
    double test_f_value = fi(test_point);
    
    // Naive backtracking with strict limit on the number of steps.
    size_t bt_iteration = 0;
    while (test_f_value > (current_f_value - bt_alpha_ * t * squared_gradient)) {
      t *= bt_beta_;
      for (size_t i = 0; i < dim; ++i) {
        test_point[i] = current_point[i] - t * current_f_gradf.gradient[i];
      }
      test_f_value = fi(test_point);
      if (bt_iteration++ == bt_max_steps_) {
        break;
      }
    }
    current_point = test_point;

    // Simple early stopping by the norm of gradient.
    if (std::sqrt(squared_gradient) < grad_eps_) {
      break;
    }
  }

 return OptimizationResult{fi(current_point), current_point};
}

template<class F>
double ConjugateGradientOptimizer<F>::PolakRibiere(const std::vector<double> &g,
                                                   const std::vector<double> &g_prev) {
  double beta = (fncas::L2Norm(g) - fncas::DotProduct(g, g_prev)) / fncas::L2Norm(g_prev);
  if (!std::isinf(beta) && !std::isnan(beta)) {
    return std::max(0.0, beta);
  } else {
    return 0.0;
  }
}

// TODO(mazhurovich): Implement more sophisticated version.
template<class F> 
OptimizationResult ConjugateGradientOptimizer<F>::Optimize(const std::vector<double>& starting_point) {
  fncas::reset_internals_singleton();
  const size_t dim = starting_point.size();
  const fncas::x gradient_helper(dim);
  fncas::f_intermediate fi(F::compute(gradient_helper));
  fncas::g_intermediate gi = fncas::g_intermediate(gradient_helper, fi);
  std::vector<double> current_point(starting_point);

  const auto initial_f_gradf = gi(current_point);
  double current_f_value = initial_f_gradf.value;
  std::vector<double> current_grad = initial_f_gradf.gradient;
  std::vector<double> s(current_grad);
  FlipSign(s);

  for (size_t iteration = 0; iteration < max_steps_; ++iteration) {
    double squared_gradient = 0.0;
    std::vector<double> test_point(dim);

    for (size_t i = 0; i < dim; ++i) {
      test_point[i] = current_point[i] + s[i];
      squared_gradient += current_grad[i] * current_grad[i];
    }

    double t = 1.0;
    double test_f_value = fi(test_point);
    
    // Naive backtracking with strict limit on the number of steps.
    size_t bt_iteration = 0;
    while (test_f_value > (current_f_value - bt_alpha_ * t * squared_gradient)) {
      t *= bt_beta_;
      for (size_t i = 0; i < dim; ++i) {
        test_point[i] = current_point[i] + t * s[i];
      }
      test_f_value = fi(test_point);
      if (bt_iteration++ == bt_max_steps_) {
        break;
      }
    }

    const auto new_f_gradf = gi(test_point);
    const std::vector<double> new_grad = new_f_gradf.gradient;
    const double omega = PolakRibiere(new_grad, current_grad);
    for (size_t i = 0; i < dim; ++i) {
      s[i] = omega * s[i] - new_grad[i];
    }
    current_point = test_point;
    current_grad = new_grad;
    current_f_value = new_f_gradf.value;

    // Simple early stopping by the norm of gradient.
    if (std::sqrt(L2Norm(s)) < grad_eps_) {
      break;
    }
  }

 return OptimizationResult{fi(current_point), current_point};
}

}  // namespace fncas

#endif
