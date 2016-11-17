/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef FASTTSV_GEN_H
#define FASTTSV_GEN_H

#include <random>
#include <string>
#include <vector>

template <typename F>
inline void CreateTSV(F&& f, size_t rows, size_t cols = 100, double scale = 10, size_t random_seed = 42) {
  std::mt19937 g(random_seed);
  std::exponential_distribution<> d(1);
  auto next_random = [&d, &g, scale]() { return static_cast<size_t>(d(g) * scale); };
  std::vector<size_t> row(cols);
  for (size_t i = 0; i < rows; ++i) {
    for (auto& col : row) {
      col = next_random();
    }
    f(row);
  }
}

#endif  // FASTTSV_GEN_H
