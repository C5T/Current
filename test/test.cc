// Can only be compiled by clang++, not g++:
// g++: "sorry, unimplemented: cannot expand ‘XS ...’ into a fixed-length argument list".

#include <cassert>
#include <iostream>
#include <sstream>
#include <vector>

#include "../src/fncas.h"

#include "boost/progress.hpp"
#include "boost/random.hpp"

const bool verbose = false;

// Abstract function definition.
// The functions to be tested defined below are be derived from this class.

class F {
 private:
  boost::mt19937 rng;
  std::vector<boost::function<double()>> p;

 protected:
  template<typename T> void add_var(T var) {
    p.push_back(boost::variate_generator<boost::mt19937&, T>(rng, var));
  }

 public:
  size_t dim() const { return p.size(); }
  virtual double iterations_coefficient() const { return 1.0; }
  std::vector<double> gen() const {
    std::vector<double> x(p.size());
    for (size_t i = 0; i < p.size(); ++i) {
      x[i] = p[i]();
    }
    return x;
  }
};

// The implementations of functions under test.

struct forty_two : F {
  virtual const char* name() const {
    return "42";
  }
  template<typename T> typename fncas::output<T>::type f(const T& x) {
    return 42;
  }
};

struct add_one : F {
  virtual const char* name() const {
    return "x + 1";
  }
  template<typename T> typename fncas::output<T>::type f(const T& x) {
    return x[0] + 1;
  }
  add_one() {
    add_var(boost::normal_distribution<double>());
  }
};

struct multiply_by_two : F {
  virtual const char* name() const {
    return "x * 2";
  }
  template<typename T> typename fncas::output<T>::type f(const T& x) {
    return x[0] * 2;
  }
  multiply_by_two() {
    add_var(boost::normal_distribution<double>());
  }
};

struct basic_arithmetics : F {
  virtual const char* name() const {
    return "a + b - c * d / e";
  }
  template<typename T> typename fncas::output<T>::type f(const T& x) {
    return x[0] + x[1] - x[2] * x[3] / x[4];
  }
  basic_arithmetics() {
    add_var(boost::normal_distribution<double>());
    add_var(boost::normal_distribution<double>());
    add_var(boost::normal_distribution<double>());
    add_var(boost::normal_distribution<double>());
    add_var(boost::exponential_distribution<double>());
  }
};

struct basic_math : F {
  virtual const char* name() const {
    return "exp(a) + log(b) + sin(c) + cos(d) + tan(e) + atan(f)";
  }
  template<typename T> typename fncas::output<T>::type f(const T& x) {
    return exp(x[0]) + log(x[1]) + sin(x[2]) + cos(x[3]) + tan(x[4]) + atan(x[5]);
  }
  basic_math() {
    add_var(boost::normal_distribution<double>());
    add_var(boost::exponential_distribution<double>());
    add_var(boost::normal_distribution<double>());
    add_var(boost::normal_distribution<double>());
    add_var(boost::normal_distribution<double>());
    add_var(boost::normal_distribution<double>());
  }
};

struct deep_function_tree : F {
  enum { DIM = 100000 };
  virtual std::string name() const {
    std::ostringstream os;
    os << "sum<i=[0.." << DIM << ")>(x[i])";
    return os.str();
  }
  template<typename T> typename fncas::output<T>::type f(const T& x) {
    typename fncas::output<T>::type tmp = 42;
    for (size_t i = 0; i < DIM; ++i) {
      tmp += x[i];
    }
    return tmp;
  }
  deep_function_tree() {
    for (size_t i = 0; i < DIM; ++i) {
      add_var(boost::normal_distribution<double>());
    }
  }
  virtual double iterations_coefficient() const { return 0.5; }
};

struct big_math : F {
  enum { DIM = 1000 };
  virtual std::string name() const {
    std::ostringstream os;
    os << "sum<i=[0.." << DIM << ")>(exp(x[i]) + log(x[i]))";
    return os.str();
  }
  template<typename T> typename fncas::output<T>::type f(const T& x) {
    typename fncas::output<T>::type tmp = 0;
    for (size_t i = 0; i < DIM; ++i) {
      tmp += exp(x[i]) + log(x[i]);
    }
    return tmp;
  }
  big_math() {
    for (size_t i = 0; i < DIM; ++i) {
      add_var(boost::exponential_distribution<double>());
    }
  }
  virtual double iterations_coefficient() const { return 1; }
};

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
        if (verbose || golden != test) {
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
      e2 = e.compile();
    }
    std::cout << "compiled, " << std::flush;
    {
      boost::progress_timer p;
      while (real_iterations--) {
        x = f.gen();
        const double golden = f.f(x);
        const double test = e2->eval(x);
        if (verbose || golden != test) {
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

int main() {
  for_each_type<
    test,
//    forty_two,
//    add_one, 
//    multiply_by_two, 
//    basic_arithmetics,
//    basic_math,
    deep_function_tree
//    big_math
    >::run();
  std::cout << "OK, all tests passed." << std::endl;
}
