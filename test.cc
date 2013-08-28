// Please see fncas.h or Makefile for build instructions.

#include <cassert>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include "fncas.h"

#define BOOST_TEST_MODULE FNCAS
#include <boost/test/included/unit_test.hpp>

template<typename T> typename fncas::output<T>::type f(const T& x) {
  return 1 + x[0] * x[1] + x[2] - x[3] + 1;
}

BOOST_AUTO_TEST_CASE(SimpleTest) {
  std::vector<double> x({5,8,9,7});
  BOOST_CHECK_EQUAL(f(x), 44);
  auto e = f(fncas::x(4));
  BOOST_CHECK_EQUAL(e.debug_as_string(), "((((1.000000+(x[0]*x[1]))+x[2])-x[3])+1.000000)");
  BOOST_CHECK_EQUAL(e.eval(x), 44);
}
