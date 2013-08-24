#include <cstdio>
#include <vector>

namespace fncas {

template<typename T> struct output {};
template<> struct output<std::vector<double> > { typedef double type; };

}

using namespace fncas;

template<typename T> typename output<T>::type f(T x) {
  return x[0] * x[1] + x[2] - x[3];
}

int main() {
  printf("%.3lf\n", f(std::vector<double>({5,8,9,7})));
}
