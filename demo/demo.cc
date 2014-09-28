#include <iostream>

#ifndef FNCAS_JIT
#define FNCAS_JIT CLANG
#endif

#include "../fncas/fncas.h"

template <typename T> static typename fncas::output<T>::type f(const T& x) {
    return x[0] + x[1];
}

int main() {
    std::cout << "Hello, FNCAS!" << std::endl;

    std::cout << "f(x) is declared as f(x[2]) = { x[0] + x[1]; };" << std::endl;

    std::cout << "Native execution:    f(1, 2) == " << f(std::vector<double>({1, 2})) << std::endl;

    auto fi = f(fncas::x(2));
    std::cout << "Intermediate format: f(1, 2) == " << fi.eval(std::vector<double>({1, 2})) << std::endl;

    auto fc = new fncas::f_compiled(fi);
    std::cout << "Compiled format:     f(1, 2) == " << fc->invoke(std::vector<double>({1, 2})) << std::endl;

    std::cout << "Intermediate details: " << fi.debug_as_string() << std::endl;
    std::cout << "Compiled details: " << fc->lib_filename() << std::endl;

    std::cout << "Done." << std::endl;
}
