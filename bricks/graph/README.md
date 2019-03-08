## Visualization Library

Bricks has C++ bindings for [`gnuplot`](http://www.gnuplot.info/), [`#include "bricks/graph/gnuplot.h"`](https://github.com/Current/C5T/blob/stable/bricks/graph/gnuplot.h) to use it.

External [`gnuplot`](http://www.gnuplot.info/) binary is invoked. The requirement is that it should be installed in the system and accessible in the `$PATH`.
```cpp
// Where visualization meets love.
using namespace current::gnuplot;
const size_t image_dim = 800;
const std::string result = GNUPlot()
  .Title("Imagine all the people ...")
  .NoKey()
  .Grid("back")
  .XLabel("... living life in peace")
  .YLabel("John Lennon, \"Imagine\"")
  .Plot(WithMeta([](Plotter p) {
    const size_t N = 1000;
    for (size_t i = 0; i < N; ++i) {
      const double t = M_PI * 2 * i / (N - 1);
      p(16 * pow(sin(t), 3),
        -(13 * cos(t) + 5 * cos(t * 2) - 2 * cos(t * 3) - cos(t * 4)));
    }
  }).LineWidth(5).Color("rgb '#FF0080'"))
  .ImageSize(image_dim)
  .OutputFormat("svg");  // Although the one below is actually a "png".
```
![](https://raw.githubusercontent.com/C5T/Current/master/bricks/graph/golden/love-Linux.png)
```cpp
// Where visualization meets science.
using namespace current::gnuplot;
const size_t image_dim = 800;
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
  .OutputFormat("svg");  // Although the one below is actually a "png".
```
![](https://raw.githubusercontent.com/C5T/Current/master/bricks/graph/golden/science-Linux.png)
```cpp
#include "../../strings/printf.h"

// Show labels on the plane.
using namespace current::gnuplot;
const size_t image_dim = 800;
const std::string result = GNUPlot()
  .Title("Labeled Points")
  .NoKey()
  .NoTics()
  .NoBorder()
  .Grid("back")
  .XRange(-1.5, +1.5)
  .YRange(-1.5, +1.5)
  .Plot(WithMeta([](Plotter& p) {
    const int N = 7;
    for (int i = 0; i < N; ++i) {
      const double phi = M_PI * 2 * i / N;
      p(cos(phi), sin(phi), current::strings::Printf("P%d", i));
    }
  }).AsLabels())
  .ImageSize(image_dim)
  .OutputFormat("svg");  // Although the one below is actually a "png".
```
![](https://raw.githubusercontent.com/C5T/Current/master/bricks/graph/golden/labels-Linux.png)
