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

#ifndef BRICKS_GRAPH_DOCU_02
#define BRICKS_GRAPH_DOCU_02

#include "../../port.h"

#ifndef CURRENT_WINDOWS

#include "../regenerate_flag.cc"

#include "../gnuplot.h"

#include "../../dflags/dflags.h"
#include "../../../3rdparty/gtest/gtest-main-with-dflags.h"

TEST(Graph, Love) {
  // Where visualization meets love.
  using namespace current::gnuplot;
#ifndef CURRENT_APPLE
const char* const formats[2] = { "gnuplot", "pngcairo" };
#else
const char* const formats[2] = { "gnuplot", "png" };
#endif
const char* const extensions[2] = { "txt", "png" };
for (size_t e = 0; e < 2; ++e) {
#if 1      
const size_t image_dim = e ? 800 : 112;
#else
  const size_t image_dim = 800;
#endif
  const std::string result = GNUPlot()
    .Title("отпусти меня")
    .NoKey()
    .Grid("back")
    .YLabel("чудо")
    .XLabel("трава")
    .Plot(WithMeta([](Plotter p) {
      const size_t N = 100001;
      for (size_t i = 0; i < N; ++i) {
        const double phi = M_PI * 2 * i / (N - 1);
        const double r = (1+sin(phi)) * (1+0.9 * cos(8*phi)) * (1+0.1*cos(24*phi));
        p(r * cos(phi), r * sin(phi));
      }
    }).LineWidth(5).Color("rgb '#168000'"))
    .ImageSize(image_dim)
#if 1      
.OutputFormat(formats[e]);
#else
    .OutputFormat("svg");  // Although the one below is actually a "png".
#endif
if (FLAGS_regenerate_golden_graphs) current::FileSystem::WriteStringToFile(result, ("golden/love-" + CURRENT_ARCH_UNAME + '.' + extensions[e]).c_str());
if (!e) ASSERT_EQ(result, current::FileSystem::ReadFileAsString("golden/love-" + CURRENT_ARCH_UNAME + '.' + extensions[e]));
}
}

#endif  // CURRENT_WINDOWS

#endif  // BRICKS_GRAPH_DOCU_02
