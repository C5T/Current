// The binary to run smoke and perf tests.
//
// TODO(dkorolev): Binary code generation on the fly is not yet implemented.
// TODO(dkorolev): Add `override` here and in other places.
//
// Generates random inputs, computes the value of the function using various
// computations techniques (native, intermediate code interpreted, code compiled
// in different languages by different compilers, machine code generation on the fly),
// compares them against each other.

// FNCAS_JIT should be set externally.
// Normally it is done by the running script since it compiles eval.cc with various settings.

#ifndef FNCAS_JIT
#error "FNCAS_JIT should be set to build eval.cc."
#endif

#include <ctime>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>
#include <random>

#include "../fncas/fncas.h"

#include <sys/time.h>

#include "functions.h"

double get_wall_time_seconds() {
  // Single-threaded implementation.
  // #include <chrono> is not friendly with clang++.
  // clock() and boost::time measure CPU time, not wall time.
  // More advanced Boost timer seems to be not present in my Ubuntu 12.04 as of 2013-09-10 - D.K.
  static struct timeval time;
  gettimeofday(&time, NULL);
  return (double)time.tv_sec + (double)time.tv_usec * 1e-6;
}

struct action {
  F* f;
  uint64_t limit_iterations;
  double limit_seconds;
  std::ostream* sout;
  std::ostream* serr;
  double duration;
  uint64_t iteration = 0;
  virtual bool run(F* f, double quantity, std::ostream* sout, std::ostream* serr) {
    this->f = f;
    limit_iterations = static_cast<uint64_t>(quantity > 0 ? quantity : 1e12);
    limit_seconds = quantity < 0 ? -quantity : 1e12;
    this->sout = sout;
    this->serr = serr;
    return do_run();
  }
  virtual bool do_run() = 0;
};

struct generic_action : action {
  virtual bool do_run() {
    start();
    double begin = get_wall_time_seconds();
    do {
      const bool result = step();
      ++iteration;
      duration = get_wall_time_seconds() - begin;
      if (!result) {
        return false;
      }
    } while (iteration < limit_iterations && duration < limit_seconds);
    return done();
  }
  virtual void start() {}
  virtual bool step() = 0;
  virtual bool done() {
    (*sout) << iteration / duration;
    return true;
  }
};

struct action_gen : generic_action {
  std::vector<double> x;
  void start() { x = std::vector<double>(f->dim()); }
  bool step() {
    f->gen(x);
    return true;
  }
};

template <typename X>
struct action_gen_eval_Xeval : generic_action, X {
  std::vector<double> x;
  std::unique_ptr<fncas::f> fncas_f;
  double compile_time;
  void start() {
    fncas_f = X::init(f);
    x = std::vector<double>(f->dim());
  }
  bool step() {
    f->gen(x);
    const double golden = f->eval_as_double(x);
    const double test = (*fncas_f)(x);
    if (test == golden) {
      return true;
    } else {
      (*serr) << golden << " != " << test << " @" << iteration;
      return false;
    }
  }
  virtual bool done() override {
    generic_action::done();
    return X::steps_done(*sout);
  }
};

// Evaluators to compare against result- and performance-wise.
struct eval {
  // Baseline code.
  struct base {
    virtual bool steps_done(std::ostream& os) { return true; }
  };
  // Native implementation calls the function natively compiled as part of the binary being run.
  struct native : base {
    std::unique_ptr<fncas::f> init(const F* f) {
      return std::unique_ptr<fncas::f>(
          new fncas::f_native(std::bind(&F::eval_as_double, f, std::placeholders::_1), f->dim()));
    }
  };
  // Intermeridate implementation calls fncas implemenation
  // that interprets the internal representation of the function.
  struct intermediate : base {
    std::unique_ptr<fncas::X> x_scope_;  // Should keep the instance of `fncas::X` in the scope.
    std::unique_ptr<fncas::f> init(const F* f) {
      x_scope_.reset(new fncas::X(f->dim()));
      return std::unique_ptr<fncas::f>(new fncas::f_intermediate(f->eval_as_expression(*x_scope_)));
    }
  };
  // Compiled implementation calls fncas implementation
  // that invokes an externally compiled version of the function.
  // The compilation takes place upon the construction of this object.
  struct compiled : base {
    std::unique_ptr<fncas::X> x_scope_;  // Should keep the instance of `fncas::X` in the scope.
    double compile_time_;
    std::unique_ptr<fncas::f> init(const F* f) {
      x_scope_.reset(new fncas::X(f->dim()));
      const double begin = get_wall_time_seconds();
      std::unique_ptr<fncas::f> result(new fncas::f_compiled(f->eval_as_expression(*x_scope_)));
      const double end = get_wall_time_seconds();
      compile_time_ = end - begin;
      return result;
    }
    virtual bool steps_done(std::ostream& os) override {
      os << ':' << compile_time_;
      return true;
    }
  };
};

typedef action_gen_eval_Xeval<eval::native> action_gen_eval_eval;
typedef action_gen_eval_Xeval<eval::intermediate> action_gen_eval_ieval;
typedef action_gen_eval_Xeval<eval::compiled> action_gen_eval_ceval;

struct action_test_gradient : generic_action {
  std::vector<double> x;
  fncas::g_approximate ga;
  fncas::g_intermediate gi;
  std::vector<double> errors;
  static double error_between(double a, double b) {
    return fabs(b - a) / std::max(1.0, std::max(fabs(a), fabs(b)));
  }
  static bool approximate_compare(double a, double b, double eps = 0.03) { return error_between(a, b) < eps; }
  void start() {
    x = std::vector<double>(f->dim());
    ga = fncas::g_approximate(std::bind(&F::eval_as_double, f, std::placeholders::_1), f->dim());
    fncas::X argument(f->dim());
    gi = fncas::g_intermediate(argument, f->eval_as_expression(argument));
  }
  bool step() {
    f->gen(x);
    std::vector<double> ra = ga(x);
    std::vector<double> ri = gi(x);
    CURRENT_ASSERT(ra.size() == ri.size());
    for (size_t i = 0; i < ra.size(); ++i) {
      errors.push_back(error_between(ra[i], ri[i]));
    }
    return true;
  }
  virtual bool done() override {
    if (errors.size() < 100) {
      (*serr) << "Not enough datapoints to test gradient.";
      return false;
    }
    std::sort(errors.begin(), errors.end());
    const double quantile = 0.95;
    const double threshold = 1e-6;
    const size_t i = static_cast<size_t>(quantile * errors.size());
    if (errors[i] > threshold) {
      (*serr) << "Error at quantile " << quantile << " is " << errors[i] << " which is above " << threshold;
      return false;
    } else {
      return true;
    }
  }
};

int main(int argc, char* argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <function> <action> <iterations or -seconds>" << std::endl;
    return -1;
  } else {
    const char* function_name = argv[1];
    const char* action_name = argv[2];
    const double quantity = (argc >= 4) ? atof(argv[3]) : 100;  // 100 iterations by default.
    F* f = registered_functions[function_name];
    if (!f) {
      std::cerr << "Function '" << function_name << "' is not defined in functions/*.h." << std::endl;
      return -1;
    } else {
      typedef std::function<int(const F*, double, std::ostream&, std::ostream&)> F_ACTION;
      std::map<std::string, std::unique_ptr<action>> actions;
      actions["gen"].reset(new action_gen());
      actions["gen_eval_eval"].reset(new action_gen_eval_eval());
      actions["gen_eval_ieval"].reset(new action_gen_eval_ieval());
      actions["gen_eval_ceval"].reset(new action_gen_eval_ceval());
      actions["test_gradient"].reset(new action_test_gradient());
      action* action_handler = actions[action_name].get();
      if (!action_handler) {
        std::cerr << "Action '" << action_name << "' is not defined." << std::endl;
        return -1;
      } else {
        std::ostringstream sout;
        std::ostringstream serr;
        sout << std::fixed << std::setprecision(5);
        serr << std::fixed << std::setprecision(5);
        if (action_handler->run(f, quantity, &sout, &serr)) {
          if (quantity < 0) {
            // Only output perf stats in perf test mode,
            // leave the output blank in smoke test mode in case of no errors.
            std::cout << sout.str() << std::endl;
          }
          return 0;
        } else {
          std::cout << serr.str() << std::endl;
          std::cerr << serr.str() << std::endl;
          return 1;
        }
      }
    }
  }
}
