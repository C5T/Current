#include <cstdio>

double f(double x[4]) {
  return x[0] * x[1] + x[2] - x[3];
}

int main() {
  double x[4] = { 5, 8, 9, 7 };
  printf("%.3lf\n", f(x));
}

