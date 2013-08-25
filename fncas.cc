// TODO(dkorolev): Move to the header.
// TODO(dkorolev): Readme, build instructions and a reference from this file.
//
// Requires:
// * Boost, sudo apt-get install libboost-dev

#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include <boost/utility.hpp>

#define DEBUG_ASSERT(expression) assert(expression)

namespace fncas {

struct a {
  std::string s;
  explicit a(const std::string& s) : s(s) {}
  a operator+(const a& rhs) const { return a('(' + s + '+' + rhs.s + ')'); }
  a operator-(const a& rhs) const { return a('(' + s + '-' + rhs.s + ')'); }
  a operator*(const a& rhs) const { return a('(' + s + '*' + rhs.s + ')'); }
};

typedef a expression;

struct x : boost::noncopyable {
  int dim;
  explicit x(int dim) : dim(dim) {
    DEBUG_ASSERT(dim > 0);
  }
  a operator[](int i) const {
    DEBUG_ASSERT(i >= 0);
    DEBUG_ASSERT(i < dim);
    std::ostringstream oss;
    oss << "x[" << i << "]";
    return a(oss.str());
  }
};

template<typename T> struct output {};
template<> struct output<std::vector<double> > { typedef double type; };
template<> struct output<x> { typedef expression type; };

}

template<typename T> typename fncas::output<T>::type f(const T& x) {
  return x[0] * x[1] + x[2] - x[3];
}

int main() {
  printf("%.3lf\n", f(std::vector<double>({5,8,9,7})));

  fncas::expression e = f(fncas::x(4));
  printf("%s\n", e.s.c_str());
}
