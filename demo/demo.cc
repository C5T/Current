#include <iostream>

#ifndef FNCAS_JIT
#define FNCAS_JIT CLANG
#endif

#include "../fncas/fncas.h"

template <typename T>
typename fncas::output<T>::type f(const T& x) {
  return (x[0] + x[1] * 2) * (x[0] + x[1] * 2);
}

int main() {
  std::cout << "Hello, FNCAS!" << std::endl;

  std::cout << "f(x) is declared as f(x[2]) = (x_0 + 2 * x_1) ^ 2);" << std::endl;

  fncas::f_native fn(f<std::vector<double>>, 2);
  std::cout << "Native execution:    f(1, 2) == " << fn(std::vector<double>({1, 2})) << std::endl;

  fncas::x x(2);
  fncas::f_intermediate fi = f(x);
  std::cout << "Intermediate format: f(1, 2) == " << fi(std::vector<double>({1, 2})) << std::endl;

  fncas::f_compiled fc = fncas::f_compiled(fi);
  std::cout << "Compiled format:     f(1, 2) == " << fc(std::vector<double>({1, 2})) << std::endl;

  std::cout << "Intermediate details: " << fi.debug_as_string() << std::endl;
  std::cout << "Compiled details: " << fc.lib_filename() << std::endl;

  auto p_3_3 = std::vector<double>({3, 3});

  fncas::g_approximate ga = fncas::g_approximate(f<std::vector<double>>, 2);
  auto d_3_3_approx = ga(p_3_3);
  std::cout << "Approximate {f, df}(3, 3) = { " << d_3_3_approx.value << ", { " << d_3_3_approx.gradient[0]
            << ", " << d_3_3_approx.gradient[1] << " } }." << std::endl;

  fncas::g_intermediate gi = fncas::g_intermediate(x, f(x));
  auto d_3_3_intermediate = gi(p_3_3);
  std::cout << "Differentiated {f, df}(3, 3) = { " << d_3_3_intermediate.value << ", { "
            << d_3_3_intermediate.gradient[0] << ", " << d_3_3_intermediate.gradient[1] << " } }." << std::endl;

  std::cout << "Done." << std::endl;
}
