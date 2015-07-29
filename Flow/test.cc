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

#include "flow.h"

#include "../3rdparty/gtest/gtest-main.h"

// `Foo`: The emitter of events. Emits the integers passed to its constructor.
CURRENT_LHS(Foo) {
  Foo() {}
  Foo(int a) { xs.push_back(a); }
  Foo(int a, int b) {
    xs.push_back(a);
    xs.push_back(b);
  }
  Foo(int a, int b, int c) {
    xs.push_back(a);
    xs.push_back(b);
    xs.push_back(c);
  }
  void run() {
    for (int e : xs) {
      emit(e);
    }
  }
  std::vector<int> xs;
};
#define Foo(...) REGISTER_CURRENT_LHS(Foo, __VA_ARGS__)

// `Bar`: The processor of events. Multiplies each integer by what was passed to its constructor.
CURRENT_VIA(Bar) {
  int k;
  Bar(int k = 1) : k(k) {}
  void f(int x) { emit(x * k); }
  void run() {}
};
#define Bar(...) REGISTER_CURRENT_VIA(Bar, __VA_ARGS__)

// `Baz`: The destination of events. Collects the output integers.
CURRENT_RHS(Baz) {
  std::ostringstream meh;
  std::ostream& os;
  Baz() : os(meh) {}
  Baz(std::ostream & os) : os(os) {}
  void f(int x) { os << x << std::endl; }
};
#define Baz(...) REGISTER_CURRENT_RHS(Baz, __VA_ARGS__)

TEST(FlowLanguage, BasicSyntax) {
  EXPECT_EQ("Foo() | ...", (Foo()).Describe());
  EXPECT_EQ("... | Bar() | ...", (Bar()).Describe());
  EXPECT_EQ("... | Baz()", (Baz()).Describe());

  EXPECT_EQ("Foo() | Baz()", (Foo() | Baz()).Describe());
  EXPECT_EQ("Foo() | Bar() | Bar() | Bar() | Baz()", (Foo() | Bar() | Bar() | Bar() | Baz()).Describe());

  EXPECT_EQ("Foo() | Bar() | ...", (Foo() | Bar()).Describe());
  EXPECT_EQ("Foo() | Bar() | Bar() | Bar() | ...", (Foo() | Bar() | Bar() | Bar()).Describe());

  EXPECT_EQ("... | Bar() | Baz()", (Bar() | Baz()).Describe());
  EXPECT_EQ("... | Bar() | Bar() | Bar() | Baz()", (Bar() | Bar() | Bar() | Baz()).Describe());

  EXPECT_EQ("... | Bar() | Bar() | Bar() | ...", (Bar() | Bar() | Bar()).Describe());

  const int blah = 5;
  EXPECT_EQ("Foo(1) | Bar(2) | Bar(3 + 4) | Bar(blah) | Baz()",
            (Foo(1) | Bar(2) | Bar(3 + 4) | Bar(blah) | Baz()).Describe());

  const int x = 1;
  const int y = 1;
  const int z = 1;
  EXPECT_EQ("Foo(x, y, z) | Baz()", (Foo(x, y, z) | Baz()).Describe());
}

TEST(FlowLanguage, SingleEdgeFlow) {
  std::ostringstream os;
  (Foo(1, 2, 3) | Baz(os)).Flow();
  EXPECT_EQ("1\n2\n3\n", os.str());
}

TEST(FlowLanguage, SingleChainFlow) {
  std::ostringstream os;
  (Foo(1, 2, 3) | Bar(10) | Bar(10) | Baz(os)).Flow();
  EXPECT_EQ("100\n200\n300\n", os.str());
}

TEST(FlowLanguage, DeclarationDoesNotRunConstructors) {
  // TODO(dkorolev): Write it.
}

TEST(FlowLanguage, BranchingSyntax) {
  // TODO(dkorolev): Implement it.

  // Foo(1) | Bar(2) + Bar(3) | Baz("A")
  // Foo(4) + Foo(5) | Bar(6) | Baz("B")
  // Foo(7) + Foo(8) + Foo(9) | Bar(10) + Bar(11) + Bar(12) | Baz("X") + Baz("Y") + Baz("Z")

  // const int XLII = 42;
  // Foo(XLII, XLII * 10, XLII * 100) | Bar(XLII + 1) + Bar(XLII + 2) + Bar(XLII + 3) | Baz(ToString(XLII))
}

TEST(FlowLanguage, BranchingFlow) {
  // TODO(dkorolev): Implement it.
}
