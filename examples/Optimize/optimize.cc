/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

// Solves the matrix game by means of a gradient-descent-based optimization.

// #define FNCAS_USE_LONG_DOUBLE

#include "../../port.h"

#ifndef FNCAS_USE_LONG_DOUBLE
#define FNCAS_JIT AS  // TODO(dkorolev) + TODO(mzhurovich): Test on Mac.
#endif  // #ifndef FNCAS_USE_LONG_DOUBLE

#include "main.h"

#include "../../FnCAS/fncas/fncas.h"

DEFINE_string(matrix, "", "If set, the JSON representation of the game matrix.");
DEFINE_size_t(dim, 4, "The dimension of square matrix A for the matrix game.");
DEFINE_int32(min, 1, "The minimum randomly generated value for A[i][j].");
DEFINE_int32(max, 9, "The minimum randomly generated value for A[i][j].");
DEFINE_uint64(seed, 19830816, "The random seed to use, zero to generate the input nondeterministically.");
DEFINE_double(eps, 1e-3, "The value of the epsilon to compare doubles with.");
DEFINE_bool(logtostderr, false, "Log to stderr.");

template <typename X>
X magic_weighting(X x) {
  return fncas::sqr(x);  // For the canonical solution, `exp(x)` would do fine; `sqr` just convereges faster. -- D.K.
}

template <typename X>
inline X simplex(X point) {
  fncas::X2V<X> sum = 0.0;
  for (auto& x : point) {
    x = magic_weighting(x);
    sum += x;
  }
  const fncas::X2V<X> k = 1.0 / sum;
  for (auto& x : point) {
    x *= k;
  }
  return point;
}

struct FunctionToOptimize {
  const size_t N;
  const std::vector<std::vector<int>>& A;

  explicit FunctionToOptimize(size_t N, const std::vector<std::vector<int>>& A) : N(N), A(A) {}

  template <typename X>
  fncas::X2V<X> ObjectiveFunction(const X& x) const {
    CURRENT_ASSERT(x.size() == N * 2);

    // Precompute strategy simplexes.
    std::vector<std::vector<fncas::X2V<X>>> proto_strategy(2, std::vector<fncas::X2V<X>>(N));
    for (size_t k = 0; k < N; ++k) {
      proto_strategy[0][k] = x[k];
      proto_strategy[1][k] = x[k + N];
    }
    std::vector<std::vector<fncas::X2V<X>>> strategy({simplex(proto_strategy[0]), simplex(proto_strategy[1])});

    // Compute the current payoff (the "equilibrium", unless the solution is not yet optimal).
    fncas::X2V<X> mixed_strategies_payoff = 0.0;
    for (size_t i = 0; i < N; ++i) {
      for (size_t j = 0; j < N; ++j) {
        mixed_strategies_payoff += strategy[0][i] * strategy[1][j] * A[i][j];
      }
    }

    // Construct the cost function that is minimized as the current point is the equlibrium one.
    const std::function<fncas::X2V<X>(fncas::X2V<X>)> softmax_penalty =
        [&](fncas::X2V<X> v) { return fncas::sqr(fncas::ramp(v)); };

    fncas::X2V<X> penalty = 0.0;

    {
      // Player one: per-opponent-strategy payoffs should be less than or equal to the `mixed_strategies_payoff`.
      for (size_t i = 0; i < N; ++i) {
        fncas::X2V<X> player_one_payoff = 0.0;
        for (size_t j = 0; j < N; ++j) {
          player_one_payoff += A[i][j] * strategy[1][j];
        }
        penalty += softmax_penalty(player_one_payoff - mixed_strategies_payoff);
      }
    }

    {
      // Player two: per-opponent-strategy payoffs should be greater than or equal to the `mixed_strategies_payoff`.
      for (size_t j = 0; j < N; ++j) {
        fncas::X2V<X> player_two_payoff = 0.0;
        for (size_t i = 0; i < N; ++i) {
          player_two_payoff += A[i][j] * strategy[0][i];
        }
        penalty += softmax_penalty(mixed_strategies_payoff - player_two_payoff);
      }
    }

    return penalty;
  }
};

template <typename T>
using optimizer_t = fncas::ConjugateGradientOptimizer<T>;  // `GradientDescentOptimizerBT`, `GradientDescentOptimizer`.

std::vector<std::vector<fncas::double_t>> solve(
    size_t N,
    const std::vector<std::vector<int>>& A,
    std::function<bool(const std::vector<std::vector<fncas::double_t>>& strategy)> validate) {
  const auto build_probabilities =
      [N](const std::vector<fncas::double_t>& x) -> std::vector<std::vector<fncas::double_t>> {
        return {simplex(std::vector<fncas::double_t>(x.begin(), x.begin() + N)),
                simplex(std::vector<fncas::double_t>(x.begin() + N, x.begin() + N * 2))};
      };

  const auto pretty_print_simplex = [](const std::vector<fncas::double_t>& x) -> std::string {
    std::ostringstream os;
    for (fncas::double_t v : x) {
      os << current::strings::Printf(" %.3lf", v);
    }
    return "{" + os.str() + " }";
  };

  std::unique_ptr<fncas::ScopedLogToStderr> scope;
  if (FLAGS_logtostderr) {
    scope = std::make_unique<fncas::ScopedLogToStderr>();
  }

  return build_probabilities(
      optimizer_t<FunctionToOptimize>(
          fncas::OptimizerParameters()
              .SetValue("max_steps", 50000)
              .SetPointBeautifier([&](const std::vector<fncas::double_t>& x) {
                const auto p = build_probabilities(x);
                return "A = " + pretty_print_simplex(p[0]) + ", B = " + pretty_print_simplex(p[1]);
              })
              .SetStoppingCriterion([&](size_t completed_iterations, const std::vector<fncas::double_t>& x) {
                static_cast<void>(completed_iterations);
                return validate(build_probabilities(x));
              }),
          N,
          A)
          .Optimize(std::vector<fncas::double_t>(N * 2, 1.0))
          .point);
}

int main(int argc, char** argv) { return run(argc, argv); }
