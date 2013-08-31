// TODO(dkorolev): Add a typelist listing all the functions under test.

// Please see fncas.h or Makefile for build instructions.

#include <cassert>
#include <iostream>
#include <vector>

#include "fncas.h"

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

// The tmplementations of functions under test.

struct forty_two : F {
  template<typename T> typename fncas::output<T>::type f(const T& x) {
    return 42;
  }
};

struct add_one : F {
  template<typename T> typename fncas::output<T>::type f(const T& x) {
    return x[0] + 1;
  }
  add_one() {
    add_var(boost::normal_distribution<double>());
  }
};

struct multiply_by_two : F {
  template<typename T> typename fncas::output<T>::type f(const T& x) {
    return x[0] * 2;
  }
  multiply_by_two() {
    add_var(boost::normal_distribution<double>());
  }
};

struct basic_arithmetics : F {
  template<typename T> typename fncas::output<T>::type f(const T& x) {
    return x[0] - x[1] * x[2] / x[3];
  }
  basic_arithmetics() {
    add_var(boost::normal_distribution<double>());
    add_var(boost::normal_distribution<double>());
    add_var(boost::normal_distribution<double>());
    add_var(boost::exponential_distribution<double>());
  }
};

struct deep_function_tree : F {
  template<typename T> typename fncas::output<T>::type f(const T& x) {
    typename fncas::output<T>::type tmp = 42;
    for (size_t i = 0; i < 100000; ++i) {
      tmp += x[0];
    }
    return tmp;
  }
  deep_function_tree() {
    add_var(boost::normal_distribution<double>());
  }
  virtual double iterations_coefficient() const { return 0.1; }
};

// Test code to compare plain function evaluation to the evaluation of its compiled form.

template<typename T> void test_eval(size_t iterations = 100) {
  T f;
  auto e = f.f(fncas::x(f.dim()));
  std::vector<double> x;
  size_t real_iterations = std::max<size_t>(3, iterations  * f.iterations_coefficient());
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
      exit(1);
    }
  }
}

// Test case runner.

int main() {
  test_eval<forty_two>();
  test_eval<add_one>();
  test_eval<multiply_by_two>();
  test_eval<basic_arithmetics>();
  test_eval<deep_function_tree>();
  std::cout << "OK" << std::endl;
}
