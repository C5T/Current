// TODO(dkorolev): Make it work.
// TODO(dkorolev): Add a script to try different compiler/optimization/JIT parameters.
// TODO(dkorolev): Get rid of function::name().

#include <cassert>
#include <iostream>
#include <sstream>
#include <vector>

#define FNCAS_JIT nasm
#include "../fncas/fncas.h"

#include "boost/progress.hpp"
#include "boost/random.hpp"

#include <boost/fusion/include/algorithm.hpp>
#include <boost/fusion/include/vector.hpp>

#include "function.h"
#include "autogen/all_functions.h"

typedef boost::fusion::vector<
#include "autogen/all_functions_list.h"
> FUNCTIONS_TYPELIST;

/*
// Test code to compare plain function evaluation vs. its "byte-code" evaluation.

template<typename T> struct test_eval {
  enum { iterations = 3 }; //10000 };
  static void run() {
    fncas::reset();
    T f;
    auto e = f.f(fncas::x(f.dim()));
    std::vector<double> x;
    size_t real_iterations = std::max<size_t>(3, iterations * f.iterations_coefficient());
    std::cout
      << "\"" << f.name() << "\", dim "
      << f.dim() << ", " << real_iterations << " iterations: " << std::flush;
    {
      boost::progress_timer p;
      while (real_iterations--) {
        x = f.gen();
        const double golden = f.f(x);
        const double test = e.eval(x);
        if (golden != test) {
          std::cout << "{ ";
          for (auto& i: x) std::cout << i << ' ';
          std::cout << "}: " << test << " vs. " << golden << std::endl;
        }
        if (golden != test) {
          std::cout << "test failed." << std::endl;
          exit(1);
        }
      }
      std::cout << "OK, ";  // boost::progress_timer takes care of the newline.
    }
  }
};

// Test code to compare plain function evaluation vs. its compiled version.

template<typename T> struct test_compiled_code_eval {
  enum { iterations = 100 }; //10000 };
  static void run() {
    fncas::reset();
    T f;
    auto e = f.f(fncas::x(f.dim()));
    std::vector<double> x;
    size_t real_iterations = std::max<size_t>(3, iterations * f.iterations_coefficient());
    std::cout
      << "\"" << f.name() << "\", dim "
      << f.dim() << ", " << real_iterations << " iterations, compiled: " << std::flush;
    std::unique_ptr<fncas::compiled_expression> e2;
    {
//      boost::progress_timer p;
      e2 = fncas::compile(e);
    }
    std::cout << "compiled, " << std::flush;
    {
      boost::progress_timer p;
      while (real_iterations--) {
        x = f.gen();
        const double golden = f.f(x);
        const double test = e2->eval(x);
        if (golden != test) {
          std::cout << "{ ";
          for (auto& i: x) std::cout << i << ' ';
          std::cout << "}: " << test << " vs. " << golden << std::endl;
        }
        if (golden != test) {
          std::cout << "test failed." << std::endl;
          exit(1);
        }
      }
      std::cout << "OK, ";  // boost::progress_timer takes care of the newline.
    }
  }
};

template<typename T> struct test {
  static void run() {
    test_eval<T>::run();
    test_compiled_code_eval<T>::run();
  }
};

template<template<typename T> class F, class X, class... XS> struct for_each_type {
  static void run() {
    F<X>::run();
    for_each_type<F, XS...>::run();
  }
};

template<template<typename T> class F, class X> struct for_each_type<F, X> {
  static void run() {
    F<X>::run();
  }
};
*/

struct run_f {
  template <typename T> void operator()(const T& t) const {
    std::cout << t.name() << std::endl;
  };
};

int main() {
  boost::fusion::for_each(FUNCTIONS_TYPELIST(), run_f());
  std::cout << "OK, all tests passed." << std::endl;
}
