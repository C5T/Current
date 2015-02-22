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

#include "docu/docu_04graph_02plotutils.cc"
#include "docu/docu_04graph_05gnuplot.cc"

#include <vector>
#include <string>

#include "gnuplot.h"
#include "plotutils.h"

#include "../3party/gtest/gtest-main.h"

using namespace bricks::plotutils;

TEST(Graph, PlotutilsPoints) {
  const int p1 = 27733;
  const int p2 = 27791;
  const double k = 1.0 / (p2 - 1);
  int tmp = 1;
  std::vector<std::pair<double, double>> flakes(200);
  for (auto& xy : flakes) {
    xy.first = k*(tmp = (tmp * p1) % p2);
    xy.second = k*(tmp = (tmp * p1) % p2);
  }
  const std::string result = Plotutils(flakes)
                                 .LineMode(LineMode::None)
                                 .GridStyle(GridStyle::None)
                                 .Symbol(Symbol::Asterisk, 0.1)
                                 .OutputFormat("png");
  ASSERT_EQ(result, bricks::FileSystem::ReadFileAsString("golden/flakes.png"));
}

TEST(Graph, PlotutilsMultiplot) {
  auto gen = [](int phi) {
    std::vector<std::pair<double, double>> xy(4);
    for (int i = 0; i <= 3; ++i) {
      const double alpha = (phi + i * 120) * M_PI / 180;
      xy[i] = std::make_pair(sin(alpha), cos(alpha));
    }
    return xy;
  };
  const std::string result =
      Plotutils({gen(0), gen(60)}).LineWidth(0.005).GridStyle(GridStyle::None).OutputFormat("png");
  ASSERT_EQ(result, bricks::FileSystem::ReadFileAsString("golden/david.png"));
}
