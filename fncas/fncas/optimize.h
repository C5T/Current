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

#ifndef FNCAS_FNCAS_OPTIMIZE_H
#define FNCAS_FNCAS_OPTIMIZE_H

#include <algorithm>
#include <map>
#include <numeric>
#include <string>
#include <type_traits>

#include "base.h"
#include "differentiate.h"
#include "exceptions.h"
#include "logger.h"
#include "mathutil.h"
#include "node.h"
#include "jit.h"

#include "../../bricks/template/decay.h"
#include "../../typesystem/struct.h"
#include "../../typesystem/optional.h"
#include "../../typesystem/helpers.h"

namespace fncas {
namespace optimize {

// clang-format off
CURRENT_STRUCT_T(ObjectiveFunctionValue) {
  using value_t = T;
  CURRENT_FIELD(value, T, T());
  CURRENT_FIELD(extra_values, (std::map<std::string, T>));
  CURRENT_CONSTRUCTOR_T(ObjectiveFunctionValue)(T value = T()) : value(value) {}
  ObjectiveFunctionValue<T>& AddPoint(const std::string& name, T value) {
    extra_values[name] = value;
    return *this;
  }
};
// clang-format on

template <typename T>
struct ObjectiveFunctionValueExtractorImpl {
  using value_t = typename T::value_t;
  static value_t Extract(const T& x) { return x.value; }
};

template <>
struct ObjectiveFunctionValueExtractorImpl<fncas::double_t> {
  using value_t = fncas::double_t;
  static value_t Extract(fncas::double_t x) { return x; }
};

template <>
struct ObjectiveFunctionValueExtractorImpl<fncas::term_t> {
  using value_t = fncas::term_t;
  static value_t Extract(fncas::term_t x) { return x; }
};

template <typename T>
typename ObjectiveFunctionValueExtractorImpl<T>::value_t ExtractValueFromObjectiveFunctionValue(const T& x) {
  return ObjectiveFunctionValueExtractorImpl<T>::Extract(x);
}

CURRENT_STRUCT(OptimizationProgress) {
  // The number of iterations the optimization took.
  CURRENT_FIELD(iterations, uint32_t, 0);

  // The value of the objective function on each iteration, `.size()` == `iterations`.
  CURRENT_FIELD(objective_function_values, std::vector<double>);

  // The values of each additional parameter, as tracked with `ObjectiveFunctionValue::AddPoint`,
  // also of `iterations` elements per parameter.
  CURRENT_FIELD(additional_values, (std::map<std::string, std::vector<double>>));

  void TrackIteration(double value) {
    ++iterations;
    objective_function_values.push_back(value);

    // If the user code returns a double, it should always do so.
    CURRENT_ASSERT(additional_values.empty());
  }

  void TrackIteration(const ObjectiveFunctionValue<double>& value) {
    ++iterations;
    objective_function_values.push_back(value.value);

    // If the user code returns `ObjectiveFunctionValue`, the set of point names added via `AddPoint()`
    // should be the same for each computation of the function.
    if (additional_values.empty()) {
      for (const auto& kv : value.extra_values) {
        additional_values[kv.first].push_back(kv.second);
      }
    } else {
      CURRENT_ASSERT(additional_values.size() == value.extra_values.size());
      for (const auto& kv : value.extra_values) {
        CURRENT_ASSERT(additional_values.count(kv.first));
        additional_values[kv.first].push_back(kv.second);
      }
    }
  }
};

CURRENT_STRUCT(OptimizationResult, ValueAndPoint) {
  CURRENT_FIELD(optimization_iterations, uint32_t, 0u);
  CURRENT_FIELD(progress, Optional<OptimizationProgress>);
  CURRENT_CONSTRUCTOR(OptimizationResult)(const ValueAndPoint& p) : SUPER(p) {}
};

enum class EarlyStoppingCriterion : bool { StopOptimization = false, ContinueOptimization = true };

inline bool NoImprovement(const ValueAndPoint& next,
                          const ValueAndPoint& current,
                          double min_relative_per_step_improvement,
                          double min_absolute_per_step_improvement) {
  if (current.value - next.value < min_absolute_per_step_improvement) {
    return true;
  }
  // Relative impovement is a tricky concept when the values are negative, close to zero, or of opposite signs.
  if (current.value > +1e-6 && next.value > +1e-6) {
    // Relative improvement condition test for positive values.
    // To the reader: substitute `next.value > current.value * ...` by `next.value / current.value > ...`.
    if (next.value > current.value * (1.0 - min_relative_per_step_improvement)) {
      return true;
    }
  } else if (current.value < -1e-6 && next.value < -1e-6) {
    // Relative improvement condition test for negative values.
    // To the reader: substitute `next.value > current.value * ...` by `next.value / current.value < ...`,
    // with inverted comparison due to the value being negative.
    if (next.value > current.value * (1.0 + min_relative_per_step_improvement)) {
      return true;
    }
  }
  return false;
}

class OptimizerParameters {
 public:
  using point_beautifier_t = std::function<std::string(const std::vector<double_t>& x)>;
  using stopping_criterion_t = std::function<EarlyStoppingCriterion(
      size_t iterations_done, const ValueAndPoint& value_and_point, const std::vector<double_t>& g)>;

  template <typename T>
  OptimizerParameters& SetValue(std::string name, T value) {
    static_assert(std::is_arithmetic_v<T>, "Value must be numeric");
    params_[name] = value;
    return *this;
  }

  template <typename T>
  const T GetValue(std::string name, T default_value) const {
    static_assert(std::is_arithmetic_v<T>, "Value must be numeric");
    if (params_.count(name)) {
      return static_cast<T>(params_.at(name));
    } else {
      return default_value;
    }
  }

  OptimizerParameters& DisableJIT() {
    jit_enabled_ = false;
    return *this;
  }

  OptimizerParameters& TrackOptimizationProgress() {
    track_optimization_progress_ = true;
    return *this;
  }

  bool IsJITEnabled() const { return jit_enabled_; }
  bool ShouldTrackProgress() const { return track_optimization_progress_; }

  OptimizerParameters& SetPointBeautifier(point_beautifier_t point_beautifier) {
    point_beautifier_ = point_beautifier;
    return *this;
  }

  point_beautifier_t GetPointBeautifier() const { return point_beautifier_; }

  OptimizerParameters& SetStoppingCriterion(stopping_criterion_t stopping_criterion) {
    stopping_criterion_ = stopping_criterion;
    return *this;
  }

  stopping_criterion_t GetStoppingCriterion() const { return stopping_criterion_; }

 private:
  std::map<std::string, double_t> params_;
  point_beautifier_t point_beautifier_;
  stopping_criterion_t stopping_criterion_;
  bool jit_enabled_ = true;
  bool track_optimization_progress_ = false;
};

// The base class for the optimizer of the function of type `F`.
template <class F, OptimizationDirection DIRECTION>
class Optimizer : impl::noncopyable {
 public:
  virtual ~Optimizer() = default;

  Optimizer() : f_instance_(std::make_unique<F>()), f_reference_(*f_instance_) {}
  Optimizer(const OptimizerParameters& parameters)
      : f_instance_(std::make_unique<F>()), f_reference_(*f_instance_), parameters_(parameters) {}

  Optimizer(F& f) : f_reference_(f) {}
  Optimizer(const OptimizerParameters& parameters, F& f) : f_reference_(f), parameters_(parameters) {}

  template <typename ARG,
            class = std::enable_if_t<!std::is_same_v<current::decay_t<ARG>, OptimizerParameters>>,
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

  const Optional<OptimizerParameters>& Parameters() const { return parameters_; }

  virtual OptimizationResult Optimize(const std::vector<double_t>& starting_point) const = 0;

  std::string PointAsString(const std::vector<double_t>& point) const {
    if (!Exists(parameters_) || !Value(parameters_).GetPointBeautifier()) {
      return JSON(std::vector<double>(point.begin(), point.end()));  // Must support other `double_t` types too. -- D.K.
    } else {
      return Value(parameters_).GetPointBeautifier()(point);
    }
  }

  EarlyStoppingCriterion StoppingCriterionSatisfied(size_t iterations_completed,
                                                    const ValueAndPoint& value_and_point,
                                                    const std::vector<double_t>& gradient) const {
    if (!Exists(parameters_) || !Value(parameters_).GetStoppingCriterion()) {
      return EarlyStoppingCriterion::ContinueOptimization;
    } else {
      return Value(parameters_).GetStoppingCriterion()(iterations_completed, value_and_point, gradient);
    }
  }

 private:
  std::unique_ptr<F> f_instance_;             // The function to optimize: instance if owned by the optimizer.
  F& f_reference_;                            // The function to optimize: reference to work with, owned or not owned.
  Optional<OptimizerParameters> parameters_;  // Optimization parameters.
};

// The generic implementation running the ultimate algorithm on intermediate or compiled versions of function+gradient.
template <typename IMPL, OptimizationDirection DIRECTION>
struct OptimizeImpl;

template <class F, OptimizationDirection DIRECTION, JIT JIT_IMPLEMENTATION, class IMPL>
class OptimizeInvoker : public Optimizer<F, DIRECTION> {
 public:
  using super_t = Optimizer<F, DIRECTION>;
  using super_t::super_t;

  OptimizationResult Optimize(const std::vector<double_t>& starting_point) const override {
    const auto& logger = impl::OptimizerLogger();
    const auto& objective_function = super_t::Function();

    const fncas::impl::X gradient_helper(starting_point.size());
    // The `ExtractValueFromObjectiveFunctionValue` construct makes sure the user-defined objective function
    // can return either `T` or `ObjectiveFunctionValue<T>`. The original code enabled `ObjectiveFunctionValue<T>`
    // to be silently cast into `T`, but that proved to be error-prone with respect to the user forgetting
    // to carry the full `ObjectiveFunctionValue<T>` type through, replacing it by a plain `T`, and losing
    // the metadata that is expected to be present at the very end.
    const fncas::impl::f_impl<JIT::Blueprint> f_i(
        ExtractValueFromObjectiveFunctionValue(objective_function.ObjectiveFunction(gradient_helper)));
    logger.Log("Optimizer: The objective function is " + current::ToString(impl::node_vector_singleton().size()) +
               " nodes.");
    if (JIT_IMPLEMENTATION != fncas::JIT::Blueprint &&
        (!Exists(super_t::Parameters()) || Value(super_t::Parameters()).IsJITEnabled())) {
      logger.Log("Optimizer: Compiling the objective function.");
#ifdef FNCAS_JIT_COMPILED
      const auto compile_f_begin_gradient = current::time::Now();
      fncas::function_t<JIT_IMPLEMENTATION> f(f_i);
      logger.Log("Optimizer: Done compiling the objective function, took " +
                 current::ToString((current::time::Now() - compile_f_begin_gradient).count() * 1e-6) + " seconds.");
      return DoOptimize(objective_function, f_i, f, starting_point, gradient_helper);
#else
      std::cerr << "Attempted to use FnCAS JIT when it's not compiled into the binary. Check your -D flags.\n";
      std::exit(-1);
#endif
    } else {
      logger.Log("Optimizer: JIT has been disabled via `DisableJIT()`, falling back to interpreted evalutions.");
      return DoOptimize(objective_function, f_i, f_i, starting_point, gradient_helper);
    }
  }

  template <typename ORIGINAL_F, typename POSSIBLY_COMPILED_F>
  OptimizationResult DoOptimize(const ORIGINAL_F& original_f,
                                const fncas::impl::f_impl<JIT::Blueprint>& f_i,
                                POSSIBLY_COMPILED_F&& f,
                                const std::vector<double_t>& starting_point,
                                const fncas::impl::X& gradient_helper) const {
    const auto& logger = impl::OptimizerLogger();

    logger.Log("Optimizer: Differentiating.");
    const fncas::impl::g_impl<fncas::JIT::Blueprint> g_i(gradient_helper, f_i);
    logger.Log("Optimizer: Augmented with the gradient the function is " +
               current::ToString(impl::node_vector_singleton().size()) + " nodes.");
    if (JIT_IMPLEMENTATION != fncas::JIT::Blueprint &&
        (!Exists(super_t::Parameters()) || Value(super_t::Parameters()).IsJITEnabled())) {
      logger.Log("Optimizer: Compiling the gradient.");
#ifdef FNCAS_JIT_COMPILED
      const auto compile_g_begin_gradient = current::time::Now();
      fncas::gradient_t<JIT_IMPLEMENTATION> g(f_i, g_i);
      logger.Log("Optimizer: Done compiling the gradient, took " +
                 current::ToString((current::time::Now() - compile_g_begin_gradient).count() * 1e-6) + " seconds.");
      return OptimizeImpl<IMPL, DIRECTION>::template RunOptimize<F>(*this, original_f, f, g, starting_point);
#else
      std::cerr << "Attempted to use FnCAS JIT when it's not compiled into the binary. Check your -D flags.\n";
      std::exit(-1);
#endif
    } else {
      return OptimizeImpl<IMPL, DIRECTION>::template RunOptimize<F>(*this, original_f, f, g_i, starting_point);
    }
  }
};

// A special implementation w/o JIT. Can compile when JIT is disabled.
template <class F, OptimizationDirection DIRECTION, class IMPL>
class OptimizeInvoker<F, DIRECTION, fncas::JIT::Blueprint, IMPL> : public Optimizer<F, DIRECTION> {
 public:
  using super_t = Optimizer<F, DIRECTION>;
  using super_t::super_t;

  OptimizationResult Optimize(const std::vector<double_t>& starting_point) const override {
    const auto& logger = impl::OptimizerLogger();
    const auto& objective_function = super_t::Function();

    const fncas::impl::X gradient_helper(starting_point.size());
    // NOTE(dkorolev): Here, `fncas::impl::X` is magically cast into `std::vector<fncas::impl::V>`.
    const fncas::impl::f_impl<JIT::Blueprint> f_i(objective_function.ObjectiveFunction(gradient_helper));
    logger.Log("Optimizer: The objective function is " + current::ToString(impl::node_vector_singleton().size()) +
               " nodes.");
    logger.Log("Optimizer: Not using JIT.");
    return DoOptimize(objective_function, f_i, f_i, starting_point, gradient_helper);
  }

  template <typename ORIGINAL_F, typename POSSIBLY_COMPILED_F>
  OptimizationResult DoOptimize(const ORIGINAL_F& original_f,
                                const fncas::impl::f_impl<JIT::Blueprint>& f_i,
                                POSSIBLY_COMPILED_F&& f,
                                const std::vector<double_t>& starting_point,
                                const fncas::impl::X& gradient_helper) const {
    const auto& logger = impl::OptimizerLogger();

    logger.Log("Optimizer: Differentiating.");
    const fncas::impl::g_impl<fncas::JIT::Blueprint> g_i(gradient_helper, f_i);
    logger.Log("Optimizer: Augmented with the gradient the function is " +
               current::ToString(impl::node_vector_singleton().size()) + " nodes.");
    return OptimizeImpl<IMPL, DIRECTION>::template RunOptimize<F>(*this, original_f, f, g_i, starting_point);
  }
};

// Naive gradient descent that tries 3 different step sizes in each iteration.
// Searches for a local minimum of `F::ObjectiveFunction` function.
struct GradientDescentOptimizerSelector;

template <class F,
          OptimizationDirection DIRECTION = OptimizationDirection::Minimize,
          JIT JIT_IMPLEMENTATION = JIT::Default>
class GradientDescentOptimizer final
    : public OptimizeInvoker<F, DIRECTION, JIT_IMPLEMENTATION, GradientDescentOptimizerSelector> {
 public:
  using super_t = OptimizeInvoker<F, DIRECTION, JIT_IMPLEMENTATION, GradientDescentOptimizerSelector>;
  using super_t::super_t;
};

template <OptimizationDirection DIRECTION>
struct OptimizeImpl<GradientDescentOptimizerSelector, DIRECTION> {
  template <typename ORIGINAL_F, typename F, typename G>
  static OptimizationResult RunOptimize(const Optimizer<ORIGINAL_F, DIRECTION>& super,
                                        const ORIGINAL_F& original_f,
                                        F&& f,
                                        G&& g,
                                        const std::vector<double_t>& starting_point) {
    const auto& logger = impl::OptimizerLogger();

    size_t max_steps = 2500;                             // Maximum number of optimization steps.
    double_t step_factor = 1.0;                          // Gradient is multiplied by this factor.
    double_t min_absolute_per_step_improvement = 1e-25;  // Terminate early if the absolute improvement is small.
    double_t min_relative_per_step_improvement = 1e-25;  // Terminate early if the relative improvement is small.
    double_t no_improvement_steps_to_terminate = 2;      // Wait for this # of consecutive no improvement iterations.

    bool track_progress = false;

    if (Exists(super.Parameters())) {
      const auto& parameters = Value(super.Parameters());
      max_steps = parameters.GetValue("max_steps", max_steps);
      step_factor = parameters.GetValue("step_factor", step_factor);
      min_relative_per_step_improvement =
          parameters.GetValue("min_relative_per_step_improvement", min_relative_per_step_improvement);
      min_absolute_per_step_improvement =
          parameters.GetValue("min_absolute_per_step_improvement", min_absolute_per_step_improvement);
      no_improvement_steps_to_terminate =
          parameters.GetValue("no_improvement_steps_to_terminate", no_improvement_steps_to_terminate);

      track_progress = parameters.ShouldTrackProgress();
    }

    logger.Log("GradientDescentOptimizer: Begin at " + super.PointAsString(starting_point));

    ValueAndPoint current(f(starting_point), starting_point);
    if (DIRECTION == OptimizationDirection::Maximize) {
      current.value *= -1;
    }

    OptimizationProgress progress;

    size_t iteration;
    int no_improvement_steps = 0;
    {
      OptimizerStats stats("GradientDescentOptimizer");
      for (iteration = 0; iteration < max_steps; ++iteration) {
        stats.JournalIteration();
        if (logger) {
          // `PointAsString()` is an expensive call, don't make it if `logger` is not initialized.
          logger.Log("GradientDescentOptimizer: Iteration " + current::ToString(iteration + 1) + ", OF to minimize = " +
                     current::ToString(current.value) + " @ " + super.PointAsString(current.point));
        }

        if (track_progress) {
          progress.TrackIteration(original_f.ObjectiveFunction(current.point));
        }

        stats.JournalGradient();
        auto gradient = g(current.point);
        if (DIRECTION == OptimizationDirection::Maximize) {
          fncas::impl::FlipSign(gradient);
        }

        if (super.StoppingCriterionSatisfied(iteration, current, gradient) ==
            EarlyStoppingCriterion::StopOptimization) {
          logger.Log("GradientDescentOptimizer: External stopping criterion satisfied, terminating.");
          break;
        }

        auto best_candidate = current;
        auto has_valid_candidate = false;
        for (const double_t step : {0.01, 0.05, 0.2}) {  // TODO(dkorolev): Something more sophisticated maybe?
          const auto candidate_point(impl::SumVectors(current.point, gradient, -step));
          stats.JournalFunction();
          double_t value = f(candidate_point);
          if (fncas::IsNormal(value)) {
            has_valid_candidate = true;
            if (DIRECTION == OptimizationDirection::Maximize) {
              value *= -1;
            }
            logger.Log("GradientDescentOptimizer: Value to minimize" + current::ToString(value) + " at step " +
                       current::ToString(step));
            best_candidate = std::min(best_candidate, ValueAndPoint(value, candidate_point));
          }
        }
        if (!has_valid_candidate) {
          CURRENT_THROW(exceptions::FnCASOptimizationException("!fncas::IsNormal(value)"));
        }
        if (NoImprovement(
                best_candidate, current, min_relative_per_step_improvement, min_absolute_per_step_improvement)) {
          ++no_improvement_steps;
          if (no_improvement_steps >= no_improvement_steps_to_terminate) {
            logger.Log("GradientDescentOptimizer: Terminating due to no improvement.");
            break;
          }
        } else {
          no_improvement_steps = 0;
        }
        current = best_candidate;
      }
    }

    if (DIRECTION == OptimizationDirection::Maximize) {
      current.value *= -1;
    }

    logger.Log("GradientDescentOptimizer: Result = " + super.PointAsString(current.point));
    logger.Log("GradientDescentOptimizer: Objective function = " + current::ToString(current.value));

    OptimizationResult result(current);
    result.optimization_iterations = static_cast<uint32_t>(iteration);

    if (track_progress) {
      result.progress = std::move(progress);
    }

    return result;
  }
};

// Simple gradient descent optimizer with backtracking algorithm.
// Searches for a local minimum of `F::ObjectiveFunction` function.
struct GradientDescentOptimizerBTSelector;

template <class F,
          OptimizationDirection DIRECTION = OptimizationDirection::Minimize,
          JIT JIT_IMPLEMENTATION = JIT::Default>
class GradientDescentOptimizerBT final
    : public OptimizeInvoker<F, DIRECTION, JIT_IMPLEMENTATION, GradientDescentOptimizerBTSelector> {
 public:
  using super_t = OptimizeInvoker<F, DIRECTION, JIT_IMPLEMENTATION, GradientDescentOptimizerBTSelector>;
  using super_t::super_t;
};

template <OptimizationDirection DIRECTION>
struct OptimizeImpl<GradientDescentOptimizerBTSelector, DIRECTION> {
  template <typename ORIGINAL_F, typename F, typename G>
  static OptimizationResult RunOptimize(const Optimizer<ORIGINAL_F, DIRECTION>& super,
                                        const ORIGINAL_F& original_f,
                                        F&& f,
                                        G&& g,
                                        const std::vector<double_t>& starting_point) {
    const auto& logger = impl::OptimizerLogger();

    size_t min_steps = 3;       // Minimum number of optimization steps (ignoring early stopping).
    size_t max_steps = 250;     // Maximum number of optimization steps.
    double_t bt_alpha = 0.5;    // Alpha parameter for backtracking algorithm.
    double_t bt_beta = 0.8;     // Beta parameter for backtracking algorithm.
    size_t bt_max_steps = 100;  // Maximum number of backtracking steps.
    double_t grad_eps = 1e-8;   // Magnitude of gradient for early stopping.
    double_t min_absolute_per_step_improvement = 1e-25;  // Terminate early if the absolute improvement is small.
    double_t min_relative_per_step_improvement = 1e-25;  // Terminate early if the relative improvement is small.
    double_t no_improvement_steps_to_terminate = 2;      // Wait for this # of consecutive no improvement iterations.

    bool track_progress = false;

    if (Exists(super.Parameters())) {
      const auto& parameters = Value(super.Parameters());
      min_steps = parameters.GetValue("min_steps", min_steps);
      max_steps = parameters.GetValue("max_steps", max_steps);
      bt_alpha = parameters.GetValue("bt_alpha", bt_alpha);
      bt_beta = parameters.GetValue("bt_beta", bt_beta);
      bt_max_steps = parameters.GetValue("bt_max_steps", bt_max_steps);
      grad_eps = parameters.GetValue("grad_eps", grad_eps);
      min_relative_per_step_improvement =
          parameters.GetValue("min_relative_per_step_improvement", min_relative_per_step_improvement);
      min_absolute_per_step_improvement =
          parameters.GetValue("min_absolute_per_step_improvement", min_absolute_per_step_improvement);
      no_improvement_steps_to_terminate =
          parameters.GetValue("no_improvement_steps_to_terminate", no_improvement_steps_to_terminate);

      track_progress = parameters.ShouldTrackProgress();
    }

    logger.Log("GradientDescentOptimizerBT: Begin at " + super.PointAsString(starting_point));

    size_t iteration;
    int no_improvement_steps = 0;

    ValueAndPoint current(f(starting_point), starting_point);

    OptimizationProgress progress;

    if (DIRECTION == OptimizationDirection::Maximize) {
      current.value *= -1;
    }

    {
      OptimizerStats stats("GradientDescentOptimizerBT");
      for (iteration = 0; iteration < max_steps; ++iteration) {
        stats.JournalIteration();
        if (logger) {
          // `PointAsString()` could be an expensive call, don't make it if `logger` is not initialized.
          logger.Log("GradientDescentOptimizerBT: Iteration " + current::ToString(iteration + 1) +
                     ", OF to minimize = " + current::ToString(current.value) + " @ " +
                     super.PointAsString(current.point));
        }

        if (track_progress) {
          progress.TrackIteration(original_f.ObjectiveFunction(current.point));
        }

        auto gradient = g(current.point);
        if (DIRECTION == OptimizationDirection::Maximize) {
          fncas::impl::FlipSign(gradient);
        }

        if (super.StoppingCriterionSatisfied(iteration, current, gradient) ==
            EarlyStoppingCriterion::StopOptimization) {
          logger.Log("GradientDescentOptimizer: External stopping criterion satisfied, terminating.");
          break;
        }

        // Simple early stopping by the norm of the gradient.
        if (std::sqrt(fncas::impl::L2Norm(gradient)) < grad_eps && iteration >= min_steps) {
          logger.Log("GradientDescentOptimizerBT: Terminating due to small gradient norm.");
          break;
        }

        fncas::impl::FlipSign(gradient);  // Going against the gradient to minimize the function.

        try {
          const auto next =
              impl::Backtracking<DIRECTION>(f, g, current.point, gradient, stats, bt_alpha, bt_beta, bt_max_steps);

          if (!IsNormal(next.value)) {
            // Would never happen as `BacktrackingException` is caught below, but just to be safe.
            CURRENT_THROW(exceptions::FnCASOptimizationException("!fncas::IsNormal(next.value)"));
          }

          if (NoImprovement(next, current, min_relative_per_step_improvement, min_absolute_per_step_improvement)) {
            ++no_improvement_steps;
            if (no_improvement_steps >= no_improvement_steps_to_terminate) {
              logger.Log("GradientDescentOptimizerBT: Terminating due to no improvement.");
              break;
            }
          } else {
            no_improvement_steps = 0;
          }

          current = next;
        } catch (const exceptions::BacktrackingException&) {
          logger.Log("GradientDescentOptimizerBT: Terminating due to no backtracking step possible.");
          break;
        }
      }
    }

    if (DIRECTION == OptimizationDirection::Maximize) {
      current.value *= -1;
    }

    logger.Log("GradientDescentOptimizerBT: Result = " + super.PointAsString(current.point));
    logger.Log("GradientDescentOptimizerBT: Objective function = " + current::ToString(current.value));

    OptimizationResult result(current);
    result.optimization_iterations = static_cast<uint32_t>(iteration);

    if (track_progress) {
      result.progress = std::move(progress);
    }

    return result;
  }
};

// Optimizer that uses a combination of conjugate gradient method and
// backtracking line search to find a local minimum of `F::ObjectiveFunction` function.
struct ConjugateGradientOptimizerSelector;

template <class F,
          OptimizationDirection DIRECTION = OptimizationDirection::Minimize,
          JIT JIT_IMPLEMENTATION = JIT::Default>
class ConjugateGradientOptimizer final
    : public OptimizeInvoker<F, DIRECTION, JIT_IMPLEMENTATION, ConjugateGradientOptimizerSelector> {
 public:
  using super_t = OptimizeInvoker<F, DIRECTION, JIT_IMPLEMENTATION, ConjugateGradientOptimizerSelector>;
  using super_t::super_t;
};

template <OptimizationDirection DIRECTION>
struct OptimizeImpl<ConjugateGradientOptimizerSelector, DIRECTION> {
  template <typename ORIGINAL_F, typename F, typename G>
  static OptimizationResult RunOptimize(const Optimizer<ORIGINAL_F, DIRECTION>& super,
                                        const ORIGINAL_F& original_f,
                                        F&& f,
                                        G&& g,
                                        const std::vector<double_t>& starting_point) {
    // TODO(mzhurovich): Implement a more sophisticated version.
    const auto& logger = impl::OptimizerLogger();

    size_t min_steps = 3;       // Minimum number of optimization steps (ignoring early stopping).
    size_t max_steps = 250;     // Maximum number of optimization steps.
    double_t bt_alpha = 0.5;    // Alpha parameter for backtracking algorithm.
    double_t bt_beta = 0.8;     // Beta parameter for backtracking algorithm.
    size_t bt_max_steps = 100;  // Maximum number of backtracking steps.
    double_t grad_eps = 1e-8;   // Magnitude of gradient for early stopping.
    double_t min_absolute_per_step_improvement = 1e-25;  // Terminate early if the absolute improvement is small.
    double_t min_relative_per_step_improvement = 1e-25;  // Terminate early if the relative improvement is small.
    double_t no_improvement_steps_to_terminate = 2;      // Wait for this # of consecutive no improvement iterations.

    bool track_progress = false;

    if (Exists(super.Parameters())) {
      const auto& parameters = Value(super.Parameters());
      min_steps = parameters.GetValue("min_steps", min_steps);
      max_steps = parameters.GetValue("max_steps", max_steps);
      bt_alpha = parameters.GetValue("bt_alpha", bt_alpha);
      bt_beta = parameters.GetValue("bt_beta", bt_beta);
      bt_max_steps = parameters.GetValue("bt_max_steps", bt_max_steps);
      grad_eps = parameters.GetValue("grad_eps", grad_eps);
      min_relative_per_step_improvement =
          parameters.GetValue("min_relative_per_step_improvement", min_relative_per_step_improvement);
      min_absolute_per_step_improvement =
          parameters.GetValue("min_absolute_per_step_improvement", min_absolute_per_step_improvement);
      no_improvement_steps_to_terminate =
          parameters.GetValue("no_improvement_steps_to_terminate", no_improvement_steps_to_terminate);

      track_progress = parameters.ShouldTrackProgress();
    }

    logger.Log("ConjugateGradientOptimizer: The objective function with its gradient is " +
               current::ToString(impl::node_vector_singleton().size()) + " nodes.");

    ValueAndPoint current(f(starting_point), starting_point);
    logger.Log("ConjugateGradientOptimizer: Original objective function = " + current::ToString(current.value));
    if (!fncas::IsNormal(current.value)) {
      CURRENT_THROW(exceptions::FnCASOptimizationException("!fncas::IsNormal(current.value)"));
    }

    OptimizationProgress progress;

    std::vector<double_t> current_gradient = g(current.point);

    if (DIRECTION == OptimizationDirection::Maximize) {
      current.value *= -1;
      fncas::impl::FlipSign(current_gradient);
    }

    std::vector<double_t> s(current_gradient);  // Direction to search for a minimum.
    fncas::impl::FlipSign(s);                   // Trying first step against the gradient to minimize the function.

    logger.Log("ConjugateGradientOptimizer: Begin at " + super.PointAsString(starting_point));
    size_t iteration;
    int no_improvement_steps = 0;
    {
      OptimizerStats stats("ConjugateGradientOptimizer");
      for (iteration = 0; iteration < max_steps; ++iteration) {
        if (super.StoppingCriterionSatisfied(iteration, current, current_gradient) ==
            EarlyStoppingCriterion::StopOptimization) {
          logger.Log("GradientDescentOptimizer: External stopping criterion satisfied, terminating.");
          break;
        }

        if (track_progress) {
          progress.TrackIteration(original_f.ObjectiveFunction(current.point));
        }

        stats.JournalIteration();
        if (logger) {
          // `PointAsString()` is an expensive call, don't make it if `logger` is not initialized.
          logger.Log("ConjugateGradientOptimizer: Iteration " + current::ToString(iteration + 1) + ", OF = " +
                     current::ToString(current.value) + " @ " + super.PointAsString(current.point));
        }
        try {
          // Backtracking line search.
          const auto next =
              impl::Backtracking<DIRECTION>(f, g, current.point, s, stats, bt_alpha, bt_beta, bt_max_steps);

          if (!IsNormal(next.value)) {
            // Would never happen as `BacktrackingException` is caught below, but just to be safe.
            CURRENT_THROW(exceptions::FnCASOptimizationException("!fncas::IsNormal(next.value)"));
          }

          stats.JournalGradient();
          auto new_gradient = g(next.point);
          if (DIRECTION == OptimizationDirection::Maximize) {
            fncas::impl::FlipSign(new_gradient);
          }

          // Calculating direction for the next step.
          const double_t omega =
              std::max(fncas::impl::PolakRibiere(new_gradient, current_gradient), static_cast<double_t>(0));
          s = impl::SumVectors(s, new_gradient, omega, -1.0);

          if (NoImprovement(next, current, min_relative_per_step_improvement, min_absolute_per_step_improvement)) {
            ++no_improvement_steps;
            if (no_improvement_steps >= no_improvement_steps_to_terminate) {
              logger.Log("ConjugateGradientOptimizer: Terminating due to no improvement.");
              break;
            }
          } else {
            no_improvement_steps = 0;
          }

          current = next;
          current_gradient = new_gradient;

          // Simple early stopping by the norm of the gradient.
          if (std::sqrt(impl::L2Norm(s)) < grad_eps && iteration >= min_steps) {
            break;
          }
        } catch (const exceptions::BacktrackingException&) {
          logger.Log("GradientDescentOptimizerBT: Terminating due to no backtracking step possible.");
          break;
        }
      }
    }

    if (DIRECTION == OptimizationDirection::Maximize) {
      current.value *= -1;
    }

    logger.Log("ConjugateGradientOptimizer: Result = " + super.PointAsString(current.point));
    logger.Log("ConjugateGradientOptimizer: Objective function = " + current::ToString(current.value));

    OptimizationResult result(current);
    result.optimization_iterations = static_cast<uint32_t>(iteration);

    if (track_progress) {
      result.progress = std::move(progress);
    }

    return result;
  }
};

template <class F,
          OptimizationDirection DIRECTION = OptimizationDirection::Minimize,
          JIT JIT_IMPLEMENTATION = JIT::Default>
using DefaultOptimizer = ConjugateGradientOptimizer<F, DIRECTION, JIT_IMPLEMENTATION>;

}  // namespace fncas::optimize
}  // namespace fncas

#endif  // #ifndef FNCAS_FNCAS_OPTIMIZE_H
