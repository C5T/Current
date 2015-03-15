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

#ifndef BRICKS_GRAPH_DOCU_07
#define BRICKS_GRAPH_DOCU_07

#include "../../port.h"

#ifndef BRICKS_WINDOWS

#include "../regenerate_flag.cc"

#include "../gnuplot.h"

#include "../../dflags/dflags.h"
#include "../../3party/gtest/gtest-main-with-dflags.h"

  #include "../../strings/printf.h"
  

TEST(Graph, GNUPlotLabels) {
  // Show labels on the plane.
  using namespace bricks::gnuplot;
const char* const extensions[2] = { "svg", "png" };
for (size_t e = 0; e < 2; ++e) {
  const std::string result = GNUPlot()
    .Title("Labeled Points")
    .NoKey()
    .Grid("back")
    .Plot(WithMeta([](Plotter& p) {
      const int N = 7;
      for (int i = 0; i < N; ++i) {
        const double phi = M_PI * 2 * i / N;
        p(cos(phi), sin(phi), bricks::strings::Printf("P%d", i));
      }
    }).AsLabels())
#if 1      
.OutputFormat(extensions[e]);
#else
    .OutputFormat("svg");
#endif
if (FLAGS_regenerate_golden_graphs) bricks::FileSystem::WriteStringToFile(result, (std::string("golden/labels.") + extensions[e]).c_str());
#ifndef BRICKS_APPLE // TODO(dkorolev): Figure out how to run this test on Apple.
if (!e) ASSERT_EQ(result, bricks::FileSystem::ReadFileAsString(std::string("golden/labels.") + extensions[e]));
#endif
}
}

#endif  // BRICKS_WINDOWS

#endif  // BRICKS_GRAPH_DOCU_05
