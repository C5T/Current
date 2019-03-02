/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2019 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

// A simple real-life-ish optimization problem to measure the performance of the native X64 JIT.
//
// The problem is effectively a sparse linear programming one with a softmax-based cost function.
// For `--n` variables and `--m` training examples, with sparseness coefficient `--k`, so that
// exactly `m * k` variables are set per each training example.
//
// Each variable has its own `true` value, which is a random integer between one and nine.
// Each training example has its own `true` right-hand-side value, which is a sum of values per variables set
// for this training example.
//
// Now, for the cost function, instead of a min-squares loss (which would make the above an LP problem, for which
// the conjugate gradient descent method is a perfect fit), this example code uses:
//
// total_penalty = sum(penalty_per_point[p])
// penalty_per_point[p] = softmax(true_value[p] - computed_value[p], 0) + softmax(computed_value[p] - true_value[p], 0)
// where softmax(a, b) = log(exp(a) + exp(b)), and thus softmax(x, 0) = log(exp(x) + 1)
//
// Since no random noise is introduced for the picture, the lowest possible `penalty_per_point[p]` is `2 * log(2)`.
// To keep things simple, in this example this value is subtracted from the cost function,
// so that ultimately it optimizes towards zero.

#include "../fncas.h"

#include "../../bricks/dflags/dflags.h"
#include "../../bricks/util/random.h"

DEFINE_int64(n, 250, "The number of training examples.");
DEFINE_int64(m, 100, "The number of variables to train on.");
DEFINE_double(k, 0.1, "The fraction of variables used per training example.");

DEFINE_string(function, "softmaxes", "The cost function to optimize for, `softmaxes/l1/l2`.");
DEFINE_string(optimizer, "jit", "The gradient evaluation technique to use `jit|as|clang|slow`.");
DEFINE_uint32(max_iterations, 10000, "The maximum number of iterations to make.");

DEFINE_bool(dump, false, "Set to dump the input data and the optimization result.");
DEFINE_bool(log, false, "Set to see the log of optimization iterations.");

DEFINE_int32(random_seed, 42, "The random seed to use. System- and compiler-dependent still.");
DEFINE_int64(a, 1, "The left end of the range to uniformly draw the true variable values from.");
DEFINE_int64(b, 9, "The [included] right end of the range to uniformly draw the true variable values from.");

struct Data {
  size_t const n;
  size_t const m;
  double const k;
  size_t const q;                                    // Just `m * k` rounded to an integer.
  std::vector<std::vector<uint32_t>> sparse_matrix;  // Sorted per training example.
  std::vector<double> true_value_per_variable;       // Integers.
  std::vector<double> true_value_per_example;        // Integers.
  std::vector<uint32_t> shuffled_variable_indexes;   // To always select exactly `m * k` variables per training example.
  Data(size_t n = FLAGS_n, size_t m = FLAGS_m, double k = FLAGS_k)
      : n(n),
        m(m),
        k(k),
        q(static_cast<size_t>(m * k + 0.5)),
        sparse_matrix(n, std::vector<uint32_t>(q)),
        true_value_per_variable(m),
        true_value_per_example(n),
        shuffled_variable_indexes(m) {
    for (size_t j = 0; j < m; ++j) {
      true_value_per_variable[j] = current::random::RandomInt(FLAGS_a, FLAGS_b);
    }
    for (size_t j = 0; j < m; ++j) {
      shuffled_variable_indexes[j] = j;
    }
    CURRENT_ASSERT(q >= 2);
    CURRENT_ASSERT(q <= m / 2);
    for (size_t i = 0; i < n; ++i) {
      std::shuffle(std::begin(shuffled_variable_indexes),
                   std::end(shuffled_variable_indexes),
                   current::random::mt19937_64_tls());
      for (size_t j = 0; j < q; ++j) {
        sparse_matrix[i][j] = shuffled_variable_indexes[j];
      }
      std::sort(std::begin(sparse_matrix[i]), std::end(sparse_matrix[i]));
      for (uint32_t const j : sparse_matrix[i]) {
        true_value_per_example[i] += true_value_per_variable[j];
      }
    }
  }
  std::vector<double> StartingPoint() const { return std::vector<double>(m, 0.5 * (FLAGS_a + FLAGS_b)); }
};

struct CostFunction {
  Data const& data;

  explicit CostFunction(Data const& data) : data(data) {}

  template <typename T>
  T ObjectiveFunction(std::vector<T> const& x) const {
    CURRENT_ASSERT(x.size() == data.m);
    T penalty = 0.0;
    for (size_t i = 0; i < data.n; ++i) {
      T value = 0.0;
      for (uint32_t const k : data.sparse_matrix[i]) {
        value += x[k];
      }
      T const delta = value - data.true_value_per_example[i];

      if (FLAGS_function == "l2") {
        // This is a case that is just easy for conjugate gradient descent: Convex multi-variable sparse min-squares.
        penalty += fncas::sqr(delta);
      } else if (FLAGS_function == "l1") {
        // FnCAS can optimize L1 as well, although analytical differentiation is kind of pointless in this case. -- D.K.
        penalty += fncas::ramp(delta) + fncas::ramp(-delta);
      } else if (FLAGS_function == "softmaxes") {
        // The cost function that shows the power of analytical differentiation.
        penalty += fncas::log(fncas::exp(delta) + 1.0) + fncas::log(fncas::exp(-delta) + 1.0);
      } else {
        std::cout << "Invalid value for `--function`, try `--help`." << std::endl;
        std::exit(-1);
      }
    }
    if (FLAGS_function == "softmaxes") {
      return penalty - (2.0 * data.n * std::log(2.0));
    } else {
      return penalty;
    }
  }
};

fncas::optimize::OptimizationResult RunOptimization(Data const& data) {
  fncas::optimize::OptimizerParameters params;
  if (FLAGS_max_iterations) {
    params.SetValue("max_steps", FLAGS_max_iterations);
  }
  if (FLAGS_optimizer == "jit") {
    return fncas::optimize::
        DefaultOptimizer<CostFunction, fncas::OptimizationDirection::Minimize, fncas::JIT::X64NativeJIT>(params, data)
            .Optimize(data.StartingPoint());
  } else if (FLAGS_optimizer == "as") {
    return fncas::optimize::DefaultOptimizer<CostFunction, fncas::OptimizationDirection::Minimize, fncas::JIT::AS>(
               params, data)
        .Optimize(data.StartingPoint());
  } else if (FLAGS_optimizer == "clang") {
    return fncas::optimize::DefaultOptimizer<CostFunction, fncas::OptimizationDirection::Minimize, fncas::JIT::CLANG>(
               params, data)
        .Optimize(data.StartingPoint());
  } else if (FLAGS_optimizer == "slow") {
    params.DisableJIT();
    return fncas::optimize::DefaultOptimizer<CostFunction>(params, data).Optimize(data.StartingPoint());
  } else {
    std::cout << "Invalid value for `--optimizer`, try `--help`." << std::endl;
    std::exit(-1);
  }
}

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  current::random::SetRandomSeed(FLAGS_random_seed);

  Data const data;

  if (FLAGS_dump) {
    std::cout << JSON(data.sparse_matrix) << std::endl;
    std::cout << JSON(data.true_value_per_variable) << std::endl;
  }

  std::unique_ptr<fncas::impl::ScopedLogToStderr> optional_log_fncas_to_stderr_scope;
  if (FLAGS_log) {
    optional_log_fncas_to_stderr_scope = std::make_unique<fncas::impl::ScopedLogToStderr>();
  }

  fncas::optimize::OptimizationResult const result = RunOptimization(data);

  if (FLAGS_dump) {
    std::cout << JSON(result) << std::endl;
  }

  if (result.value < 1e-5) {
    std::cout << "OK, test passed." << std::endl;
  } else {
    std::cout << "FAIL, the optimization did not converge." << std::endl;
  }
}
