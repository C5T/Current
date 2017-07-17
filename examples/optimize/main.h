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

// Simple tests:
//
// ./$BINARY --matrix '[[-1,1],[3,-1]]'               # Simple asymmetric 'coin flip' game.
// ./$BINARY --matrix '[[0,-1,3],[1,0,-1],[-1,1,0]]'  # Simple asymmetric 'rock-paper-scissors' game.

#ifndef EXAMPLES_OPTIMIZE_MAIN_H
#define EXAMPLES_OPTIMIZE_MAIN_H

#include "../../bricks/dflags/dflags.h"
#include "../../bricks/util/random.h"
#include "../../typesystem/serialization/json.h"
#include "../../fncas/fncas/base.h"  // typename `fncas::double_t`.

DECLARE_string(matrix);
DECLARE_size_t(dim);
DECLARE_int32(min);
DECLARE_int32(max);
DECLARE_uint64(seed);
DECLARE_double(eps);

std::vector<std::vector<fncas::double_t>> solve(
    size_t N,
    const std::vector<std::vector<int>>& A,
    std::function<bool(const std::vector<std::vector<fncas::double_t>>& strategy)> validate);

inline bool validate(size_t N,
                     const std::vector<std::vector<int>>& A,
                     const std::vector<std::vector<fncas::double_t>>& strategy,
                     bool print) {
  // Make sure the solution is valid.
  if (strategy.size() != 2u || strategy[0].size() != N || strategy[1].size() != N) {
    std::cerr << "The resulting strategies matrix should be of size 2 x N." << std::endl;
    std::exit(-1);
  }

  // Compute the equilibrium
  const fncas::double_t equilibrium = [&]() {
    fncas::double_t r = 0.0;
    for (size_t i = 0; i < N; ++i) {
      for (size_t j = 0; j < N; ++j) {
        r += A[i][j] * strategy[0][i] * strategy[1][j];
      }
    }
    return r;
  }();

  if (print) {
    std::cout << "### Equilibrium\n\nPayoff: `" << equilibrium << "`\n\n";
  }

  // Validate the solution. Each individual player one's strategy should result in the payoff no greater
  // than the equlibrium one, and each individual player two's strategy should result in the payoff no less
  // then the equilibrium one -- with certain threshold as the solution found by this algorithm is approximate.
  {
    // Validate for player one.
    for (size_t i = 0; i < N; ++i) {
      fncas::double_t player_one_payoff = 0.0;
      for (size_t j = 0; j < N; ++j) {
        player_one_payoff += A[i][j] * strategy[1][j];
      }
      if (player_one_payoff > equilibrium + FLAGS_eps) {
        if (print) {
          std::cerr << "Fail: player one's strategy " << i << " results in payoff " << player_one_payoff
                    << ", which is greater than the equilibrium " << equilibrium << std::endl;
        }
        return false;
      }
    }
  }
  {
    // Validate for player two.
    for (size_t j = 0; j < N; ++j) {
      fncas::double_t player_two_payoff = 0.0;
      for (size_t i = 0; i < N; ++i) {
        player_two_payoff += A[i][j] * strategy[0][i];
      }
      if (player_two_payoff < equilibrium - FLAGS_eps) {
        if (print) {
          std::cerr << "Fail: player two's strategy " << j << " results in payoff " << player_two_payoff
                    << ", which is less than the equilibrium " << equilibrium << std::endl;
        }
        return false;
      }
    }
  }

  return true;
}

inline int run(int& argc, char**& argv) {
  ParseDFlags(&argc, &argv);

  size_t N;
  std::vector<std::vector<int>> A;

  if (FLAGS_matrix.empty()) {
    // Generate the game matrix randomly.
    N = static_cast<size_t>(FLAGS_dim);
    A = std::vector<std::vector<int>>(N, std::vector<int>(N));
    if (FLAGS_seed) {
      SetRandomSeed(FLAGS_seed);
    }
    for (auto& row : A) {
      for (int& e : row) {
        e = RandomInt(FLAGS_min, FLAGS_max);
      }
    }
  } else {
    // Parse the game matrix from the command line flag.
    try {
      ParseJSON(FLAGS_matrix, A);
    } catch (const current::Exception& e) {
      std::cerr << "Exception: " << e.DetailedDescription() << std::endl;
      return -1;
    }
    if (A.empty()) {
      std::cerr << "The matrix should be nonempty." << std::endl;
      return -1;
    }
    N = A.size();
    for (const auto& row : A) {
      if (row.size() != N) {
        std::cerr << "The matrix should be square, N = " << N << ", bad row: " << JSON(row) << std::endl;
        return -1;
      }
    }
  }

  std::cout << "### Game Matrix" << std::endl;
  std::cout << "```" << std::endl;
  for (const auto& row : A) {
    for (const int e : row) {
      std::cout << e << ' ';
    }
    std::cout << '\n';
  }
  std::cout << "```\n" << std::endl;

  // Call the user code to solve the game.
  const auto strategy = solve(N,
                              A,
                              [&](const std::vector<std::vector<fncas::double_t>>& strategy)
                                  -> bool { return validate(N, A, strategy, false); });

  // Make sure the solution is valid.
  if (strategy.size() != 2u || strategy[0].size() != N || strategy[1].size() != N) {
    std::cerr << "The resulting strategies matrix should be of size 2 x N." << std::endl;
    return 1;
  }

  for (size_t p = 0; p < 2; ++p) {
    fncas::double_t sum = 0.0;
    for (fncas::double_t x : strategy[p]) {
      if (x < 0) {
        std::cerr << "Negative probability in the output." << std::endl;
        return 1;
      }
      sum += x;
    }
    if (std::fabs(sum - 1.0) > 1e-6) {
      std::cerr << "The probabilities in the output do not sum up to one." << std::endl;
      return 1;
    }
  }

  // Print the solution.
  std::cout << "### Strategies\n";
  std::cout << "#### Player One\n```\n";
  for (fncas::double_t p : strategy[0]) {
    std::cout << (p > 5e-3 ? Printf("%.3lf\n", p) : "-\n");
  }
  std::cout << "```\n#### Player Two\n```\n";
  for (fncas::double_t p : strategy[1]) {
    std::cout << (p > 5e-3 ? Printf("%.3lf\n", p) : "-\n");
  }
  std::cout << "```\n\n";

  if (validate(N, A, strategy, true)) {
    // All checks have passes, the solution is correct.
    std::cout << "#### Validation\n\nOK." << std::endl;
    return 0;
  } else {
    std::cout << "#### Fail\n\nValidation failed." << std::endl;
    return 1;
  }
}

#endif  // EXAMPLES_OPTIMIZE_MAIN_H
