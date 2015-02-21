## Visualization Library

Bricks has C++ bindings for [`plotutils`](http://www.gnu.org/software/plotutils/) and [`gnuplot`](http://www.gnuplot.info/). Use [`#include "Bricks/graph/plotutils.h"`](https://github.com/KnowSheet/Bricks/blob/master/graph/plotutils.h) and [`#include "Bricks/graph/gnuplot.h"`](https://github.com/KnowSheet/Bricks/blob/master/graph/gnuplot.h).

The [`plotutils`](http://www.gnu.org/software/plotutils/) tool is somewhat simpler and lighter. The [`gnuplot`](http://www.gnuplot.info/) one is more scientific and offers a wider range of features.

Both libraries are invoked as external system calls. Make sure they are installed in the system that runs your code.
