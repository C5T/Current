#include <cstdio>
#include <vector>

double f(const std::vector<double>& x) {
  return x[0] * x[1] + x[2] - x[3];
}

int main() {
  printf("%.3lf\n", f({5,8,9,7}));
}

