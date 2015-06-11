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

#ifndef BRICKS_GRAPH_DOCU_05
#define BRICKS_GRAPH_DOCU_05

#include "../../port.h"

#ifndef BRICKS_WINDOWS

#include "../regenerate_flag.cc"

#include "../gnuplot.h"

#include "../../dflags/dflags.h"
#include "../../../3rdparty/gtest/gtest-main-with-dflags.h"

TEST(Graph, GNUPlotScience) {
  // Where visualization meets science.
  using namespace bricks::gnuplot;
#ifndef BRICKS_APPLE
const char* const formats[2] = { "dumb", "pngcairo" };
#else
const char* const formats[2] = { "dumb", "png" };
#endif
const char* const extensions[2] = { "txt", "png" };
for (size_t e = 0; e < 2; ++e) {
#if 1      
const size_t image_dim = e ? 800 : 112;
#else
  const size_t image_dim = 800;
#endif
  const std::string result = GNUPlot()
    .Title("Graph 'title' with various \"quotes\"")
    .KeyTitle("'Legend', also known as the \"key\"")
    .XRange(-5, +5)
    .YRange(-2, +2)
    .Grid("back")
    .Plot([](Plotter& p) {
      for (int i = -50; i <= +50; ++i) {
        p(0.1 * i, ::sin(0.1 * i));
      }
    })
    .Plot(WithMeta([](Plotter& p) {
                     for (int i = -50; i <= +50; ++i) {
                       p(0.1 * i, ::cos(0.1 * i));
                     }
                   })
              .AsPoints()
              .Color("rgb 'blue'")
              .Name("\"cos(x)\", '.AsPoints().Color(\"rgb 'blue'\")'"))
    .ImageSize(image_dim)
#if 1      
.OutputFormat(formats[e]);
#else
    .OutputFormat("svg");  // Although the one below is actually a "png".
#endif
if (FLAGS_regenerate_golden_graphs) bricks::FileSystem::WriteStringToFile(result, ("golden/science-" + BRICKS_ARCH_UNAME + '.' + extensions[e]).c_str());
if (!e) ASSERT_EQ(result, bricks::FileSystem::ReadFileAsString("golden/science-" + BRICKS_ARCH_UNAME + '.' + extensions[e]));
}
}

#endif  // BRICKS_WINDOWS

#endif  // BRICKS_GRAPH_DOCU_05
