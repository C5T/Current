// TODO(dkorolev): Readme, build instructions and a reference from this file.
//
// Please see fncas.h or Makefile for build instructions.

#include <cassert>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include "fncas.h"

template<typename T> typename fncas::output<T>::type f(const T& x) {
  if (false) {
    return 1 + x[0] * x[1] + x[2] - x[3] + 1;
  } else {
    typename fncas::output<T>::type tmp;
    tmp = 42;
    // Change 100 to 1000000 to have SEGFAULT due to stack overflow because of DFS.
    for (int i = 0; i < 1000000; ++i) {
      tmp += x[0];
    }
    return tmp;
  }
}

int main() {
  std::vector<double> x;
  x.push_back(5);
  x.push_back(8);
  x.push_back(9);
  x.push_back(7);
  printf("%lf\n", f(x));
  auto e = f(fncas::x(4));
//  printf("%s\n", e.debug_as_string().c_str());
  printf("%lf\n", e.eval(x));
}
