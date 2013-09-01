/*
top-level: compile our code with clang++ or g++
top-level: compile our code with and without optimizations

mid-level: measure time for { just generate input data, generate+basic computation, generate+bytecode computation, generate+JIT{nasm,clang}}

low-level: across all functions


./test $function $action $seconds
*/

// TODO(dkorolev): Make it work.
// TODO(dkorolev): Add a script to try different compiler/optimization/JIT parameters.
// TODO(dkorolev): Get rid of function::name().

#include <cassert>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

#define FNCAS_JIT nasm
#include "../fncas/fncas.h"

#include "boost/progress.hpp"
#include "boost/random.hpp"

#include "function.h"
#include "autogen/functions.h"

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

int main(int argc, char* argv[]) {
  for (auto cit : registered_functions) {
    std::cout << cit.first << " -> " << cit.second->dim() << std::endl;
  }
  std::cout << "OK, all tests passed." << std::endl;
}
