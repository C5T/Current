// TODO(dkorolev): Add a script to try different compiler/optimization/JIT parameters.

#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>

#define FNCAS_JIT nasm
#include "../fncas/fncas.h"

//#include "boost/progress.hpp"
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
*/

struct action_base {
  virtual void run(const F* f, double seconds) = 0;
};

struct action_generate : action_base {
  virtual void run(const F* f, double seconds) {
    std::chrono::high_resolution_clock timer;
    double duration;
    size_t iterations = 0;
    std::vector<double> x(f->dim());
    auto begin = timer.now();
    do {
      f->gen(x);
      duration =
        std::chrono::duration_cast<std::chrono::duration<int, std::milli>>(timer.now() - begin).count() * 0.001;
      ++iterations;
    } while (duration < seconds);
    std::cout << std::fixed << std::setprecision(5) << iterations / duration << std::endl;
  }
};

struct action_evaluate : action_base {
  virtual void run(const F* f, double seconds) {
    std::chrono::high_resolution_clock timer;
    std::vector<double> x(f->dim());
    std::vector<double> results;
    results.reserve(10000000);
    double duration;
    auto begin = timer.now();
    do {
      f->gen(x);
      duration =
        std::chrono::duration_cast<std::chrono::duration<int, std::milli>>(timer.now() - begin).count() * 0.001;
      results.push_back(f->eval_double(x));
    } while (duration < seconds);
    // TODO(dkorolev): Reset pseudo-random seed and compare the results.
    std::cout << std::fixed << std::setprecision(5) << results.size() / duration << std::endl;
  }
};

struct action_intermediate_evaluate : action_base {
  virtual void run(const F* f, double seconds) {
    std::chrono::high_resolution_clock timer;
    std::vector<double> x(f->dim());
    std::vector<double> results;
    results.reserve(10000000);
    double duration;
    auto expression = f->eval_expression(fncas::x(f->dim()));
    auto begin = timer.now();
    do {
      f->gen(x);
      duration =
        std::chrono::duration_cast<std::chrono::duration<int, std::milli>>(timer.now() - begin).count() * 0.001;
      results.push_back(expression.eval(x));
    } while (duration < seconds);
    // TODO(dkorolev): Reset pseudo-random seed and compare the results.
    std::cout << std::fixed << std::setprecision(5) << results.size() / duration << std::endl;
  }
};

int main(int argc, char* argv[]) {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " <function> <action> <seconds>" << std::endl;
    return -1;
  } else {
    const F* f = registered_functions[argv[1]];
    if (!f) {
      std::cerr << "Function '" << argv[1] << "' is not defined in out functions/*.h." << std::endl;
      return -1;
    } else {
      action_generate generate;
      action_evaluate evaluate;
      action_intermediate_evaluate intermediate_evaluate;
      std::map<std::string, action_base*> actions;
      actions["generate"] = &generate;
      actions["evaluate"] = &evaluate;
      actions["intermediate_evaluate"] = &intermediate_evaluate;
      action_base* action = actions[argv[2]];
      if (!action) {
        std::cerr << "Action '" << argv[2] << "' is not defined." << std::endl;
        return -1;
      } else {
        action->run(f, atof(argv[3]));
        return 0;
      }
    }
  }
}
