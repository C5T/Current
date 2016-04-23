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

#include "graphviz.h"

#include "../dflags/dflags.h"
#include "../file/file.h"

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_bool(graphviz_overwrite_golden_files,
            false,
            "Set to true to have SVG golden files created/overwritten.");

template <current::graphviz::GraphDirected DIRECTED>
inline void RunTest(const current::graphviz::GenericGraph<DIRECTED>& graph) {
  using current::FileSystem;
  using namespace current::graphviz;

  const auto filename_prefix = FileSystem::JoinPath("golden", CurrentTestName());
  if (FLAGS_graphviz_overwrite_golden_files) {
    FileSystem::WriteStringToFile(graph.AsDOT(), (filename_prefix + ".dot").c_str());
    FileSystem::WriteStringToFile(graph.AsSVG(), (filename_prefix + ".svg").c_str());
  }

  EXPECT_EQ(FileSystem::ReadFileAsString(filename_prefix + ".dot"), graph.AsDOT());
}

TEST(GraphViz, Smoke) { RunTest(current::graphviz::Graph().Add(current::graphviz::Node("smoke"))); }

TEST(GraphViz, HelloWorld) {
  using namespace current::graphviz;

  Node hello("Hello");
  Node world("World");

  Graph g;
  g += hello;
  g += world;
  g += Edge(hello, world);

  RunTest(g);
}

TEST(GraphViz, TextEscaping) {
  using namespace current::graphviz;

  Graph g;
  g += Node("plain");
  g += Node("with whitespace");
  g += Node("multi\nline");
  g += Node("contains 'single-quoted' text");
  g += Node("contains \"double-quoted\" text");
  RunTest(g);
}

TEST(GraphViz, DirectedHelloWorld) {
  using namespace current::graphviz;

  Node hello("Hello");
  Node current("Current");
  Node world("World");

  hello.Shape("blah").Shape(nullptr);  // Set and erase.
  current["shape"] = "box";
  world.Shape("octagon");

  DiGraph g;
  g.RankDirLR();
  g += hello;
  g += current;
  g += world;
  g += Edge(hello, world);
  g += Edge(hello, current);
  g += Edge(current, world);

  RunTest(g);
}

TEST(GraphViz, Hyperlink) {
  using namespace current::graphviz;

  RunTest(DiGraph().Add(Node("Click!").Shape("box").Set("URL", "http://current.ai").Set("fontcolor", "blue")));
}

TEST(GraphViz, Record) {
  using namespace current::graphviz;

  RunTest(DiGraph()
              .Add(Node("horizontal|does|work").Shape("record"))
              .Add(Node("{vertical|works|too}").Shape("record"))
              .Add(Node("mixed|{can|be|used}|as well").Shape("record")));
}

TEST(GraphViz, Grouping) {
  using namespace current::graphviz;

  Node a("a");
  Node b("b");
  Node c("c");
  Node d("d");
  Node e("e");
  Node f("f");

  DiGraph g;

  g += a;
  g += b;
  g += c;
  g += d;
  g += e;
  g += f;

  g += Group().Add(a).Add(c).Add(d);

  g += Edge(a, b);
  g += Edge(a, c);
  g += Edge(a, d);
  g += Edge(b, e);
  g += Edge(b, f);
  g += Edge(c, e);

  RunTest(g);
}
