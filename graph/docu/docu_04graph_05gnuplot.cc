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

#include "../gnuplot.h"

#include "../../3party/gtest/gtest-main.h"

TEST(Graph, GNUPlotScience) {
  // Where visualization meets science.
  using namespace bricks::gnuplot;
  const std::string result = GNUPlot()
    .Title("Foo 'bar' \"baz\"")
    .KeyTitle("Meh 'in' \"quotes\"")
    .XRange(-42, 42)
    .YRange(-2.5, +2.5)
    .Grid("back")
    .Plot([](Plotter& p) {
      for (int i = -100; i <= +100; ++i) {
        p(i, ::sin(0.1 * i));
      }
    })
    .Plot(WithMeta([](Plotter& p) {
                     for (int i = -100; i <= +100; ++i) {
                       p(i, ::cos(0.1 * i));
                     }
                   })
              .Name("\"Cosine\" as 'points'")
              .AsPoints())
    .OutputFormat("svg");
ASSERT_EQ(result, bricks::FileSystem::ReadFileAsString("golden/gnuplot.svg"));
}

#endif  // BRICKS_GRAPH_DOCU_05
