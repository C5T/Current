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

// Solves the matrix game using the Brown-Robinson iterative method.
// Note: The convergence of this method is very slow. This implementation is only meant to be used as a smoke test.

#include "main.h"

DEFINE_string(matrix, "", "If set, the JSON representation of the game matrix.");
DEFINE_size_t(dim, 4, "The dimension of square matrix A for the matrix game.");
DEFINE_int32(min, 1, "The minimum randomly generated value for A[i][j].");
DEFINE_int32(max, 9, "The minimum randomly generated value for A[i][j].");
DEFINE_uint64(seed, 19830816, "The random seed to use, zero to generate the input nondeterministically.");
DEFINE_double(eps, 1e-3, "The acceptable threshold between the equilibrium payoff and the worst outliner.");
DEFINE_bool(logtostderr, false, "Log to stderr.");

DEFINE_size_t(iterations_between_validations,
              510510,  // 2*3*5*7*11*13*17
              "Every how many iterations run should the solution be tested for optimality.");

std::vector<std::vector<double>> solve(size_t N,
                                       const std::vector<std::vector<int>>& A,
                                       std::function<bool(const std::vector<std::vector<double>>& strategy)> validate) {
  // S[0/1]: The strategies of the two players respectively. Normalize by `sum(S[player][*])` to get the simplex.
  std::vector<std::vector<int64_t>> S(2, std::vector<int64_t>(N, 1));

  // P[0/1]: The payoffs "against" each player given their present strategy.
  // Effectively, the product of the matrix `A` and the player's strategy.
  std::vector<std::vector<int64_t>> P(2, std::vector<int64_t>(N, 0));

  // Compute the intial state of payoffs.
  // The payoff of player two's strategy `j` against current `S[0]` is `P[0][j] = sum_i(A[i][j] * S[0][i])`.
  // The payoff of player one's strategy `i` against current `S[1]` is `P[1][i] = sum_j(A[i][j] * S[1][j])`.
  for (size_t i = 0; i < N; ++i) {
    for (size_t j = 0; j < N; ++j) {
      P[0][j] += A[i][j];
      P[1][i] += A[i][j];
    }
  }

  // Run optimization iterations maintaining the invariants of `S` and `P`.
  size_t total_iterations = 0;
  const auto iteration = [&]() {
    ++total_iterations;
    {
      // See how can player one do better.
      size_t best_i = 0;
      int64_t max_payoff = P[1][0];
      for (size_t i = 1; i < N; ++i) {
        // Player one is maximizing their payoff from player two.
        if (P[1][i] > max_payoff) {
          best_i = i;
          max_payoff = P[1][i];
        }
      }
      // Player one adds strategy `best_i` into their mix.
      ++S[0][best_i];
      for (size_t j = 0; j < N; ++j) {
        P[0][j] += A[best_i][j];
      }
    }
    {
      // See how can player two do better.
      size_t best_j = 0;
      int64_t min_payoff = P[0][0];
      for (size_t j = 1; j < N; ++j) {
        // Player two is minimizing their payoff to player one.
        if (P[0][j] < min_payoff) {
          best_j = j;
          min_payoff = P[0][j];
        }
      }
      // Player two adds strategy `best_j` into their mix.
      ++S[1][best_j];
      for (size_t i = 0; i < N; ++i) {
        P[1][i] += A[i][best_j];
      }
    }
  };

  // Snapshots of `S` every half `--iterations_between_validations`.
  std::vector<std::vector<std::vector<int64_t>>> counters;
  const size_t half_number_of_iterations = std::max(static_cast<size_t>(1), FLAGS_iterations_between_validations);

  // Run the incrementally improving iterations.
  const auto begin_timestamp = current::time::Now();
  while (true) {
    for (size_t i = 0; i < half_number_of_iterations; ++i) {
      iteration();
    }
    counters.push_back(S);
    if (counters.size() >= 3 && (counters.size() % 2) == 1) {
      // Compute and return the final strategy.
      const size_t c = counters.size() - 1u;  // `c` is even.
      const auto& last = counters[c];
      const auto& mid = counters[c / 2];
      std::vector<std::vector<double>> strategy(2, std::vector<double>(N));
      const double k = 1.0 / (half_number_of_iterations * (c / 2));
      for (size_t p = 0; p < 2; ++p) {
        for (size_t i = 0; i < N; ++i) {
          strategy[p][i] = (last[p][i] - mid[p][i]) * k;
        }
      }
      // If the current strategy is the optimal one, we're done. Otherwise, keep iterating.
      if (validate(strategy)) {
        if (FLAGS_logtostderr) {
          std::cerr << "Done in " << total_iterations << " iterations, "
                    << total_iterations / (1e-6 * (current::time::Now() - begin_timestamp).count()) << " per second."
                    << std::endl;
        }
        return strategy;
      }
    }
  }
}

int main(int argc, char** argv) { return run(argc, argv); }
