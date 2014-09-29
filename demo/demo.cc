#include <iostream>

#ifndef FNCAS_JIT
#define FNCAS_JIT CLANG
#endif

#include "../fncas/fncas.h"

template <typename T> typename fncas::output<T>::type f(const T& x) {
  return (x[0] + x[1] * 2) * (x[0] + x[1] * 2);
}

int main() {
  std::cout << "Hello, FNCAS!" << std::endl;

  std::cout << "f(x) is declared as f(x[2]) = (x_0 + 2 * x_1) ^ 2);" << std::endl;

  std::cout << "Native execution:    f(1, 2) == " << f(std::vector<double>({1, 2})) << std::endl;

  fncas::f_intermediate fi = f(fncas::x(2));
  std::cout << "Intermediate format: f(1, 2) == " << fi(std::vector<double>({1, 2})) << std::endl;

  fncas::f_compiled fc = fncas::f_compiled(fi);
  std::cout << "Compiled format:     f(1, 2) == " << fc(std::vector<double>({1, 2})) << std::endl;

  std::cout << "Intermediate details: " << fi.debug_as_string() << std::endl;
  std::cout << "Compiled details: " << fc.lib_filename() << std::endl;

  fncas::f_intermediate df_by_x0 = fi.differentiate(0);
  fncas::f_intermediate df_by_x1 = fi.differentiate(1);
  std::cout << "d(f) / d(x[0]): " << df_by_x0.debug_as_string() << std::endl;
  std::cout << "d(f) / d(x[1]): " << df_by_x1.debug_as_string() << std::endl;

  auto p_3_3 = std::vector<double>({3, 3});
  std::cout << "df(3, 3) = { " << df_by_x0(p_3_3) << ", " << df_by_x1(p_3_3) << " }." << std::endl;

  auto double_f = f<std::vector<double>>;
  auto d_3_3_approx = std::vector<double>(
      {fncas::approximate_derivative(double_f, p_3_3, 0), fncas::approximate_derivative(double_f, p_3_3, 1)});
  std::cout << "approximate df(3, 3) = { " << d_3_3_approx[0] << ", " << d_3_3_approx[1] << " }." << std::endl;

  std::cout << "Done." << std::endl;
}
