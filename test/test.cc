/*
# To test on a fresh Ubuntu machine run the following.
sudo apt-get update
sudo apt-get install -y git build-essential libboost-dev nasm clang
git clone https://github.com/dkorolev/fncas.git
cd fncas/fncas
make
cd -
cd fncas/test
./run_test.sh
cd -
# Examine fncas/test/report.html afterwards.
*/

// Normally FNCAS_JIT is set by run_test.sh that runs is with various compilation options.
#ifndef FNCAS_JIT
#error "FNCAS_JIT should be set to build test.cc."
#endif

#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

#include "../fncas/fncas.h"

#include "boost/random.hpp"

#include "function.h"
#include "autogen/functions.h"

bool gen(const F* f, size_t max_iterations, double max_seconds, std::ostream& sout, std::ostream& serr) {
  std::chrono::high_resolution_clock timer;
  double duration;
  std::vector<double> x(f->dim());
  size_t iteration = 0;
  auto begin = timer.now();
  do {
    f->gen(x);
    duration =
      std::chrono::duration_cast<std::chrono::duration<int, std::milli>>(timer.now() - begin).count() * 0.001;
    ++iteration;
  } while (duration < max_seconds && iteration < max_iterations);
  sout << iteration / duration;
  return true;
}

bool gen_eval(const F* f, size_t max_iterations, double max_seconds, std::ostream& sout, std::ostream& serr) {
  std::chrono::high_resolution_clock timer;
  std::vector<double> x(f->dim());
  double duration;
  size_t iteration = 0;
  auto begin = timer.now();
  do {
    f->gen(x);
    f->eval_double(x);
    duration =
      std::chrono::duration_cast<std::chrono::duration<int, std::milli>>(timer.now() - begin).count() * 0.001;
      ++iteration;
  } while (duration < max_seconds && iteration < max_iterations);
  sout << iteration / duration;
  return true;
}

bool gen_eval_ieval(const F* f, size_t max_iterations, double max_seconds, std::ostream& sout, std::ostream& serr) {
  std::chrono::high_resolution_clock timer;
  std::vector<double> x(f->dim());
  double duration;
  auto intermediate = f->eval_expression(fncas::x(f->dim()));
  size_t iteration = 0;
  auto begin = timer.now();
  do {
    f->gen(x);
    duration =
      std::chrono::duration_cast<std::chrono::duration<int, std::milli>>(timer.now() - begin).count() * 0.001;
    const double golden = f->eval_double(x);
    const double test = intermediate.eval(x);
    if (test != golden) {
      serr << golden << " != " << test << " @" << iteration;
      return false;
    }
    ++iteration;
  } while (duration < max_seconds && iteration < max_iterations);
  sout << iteration / duration;
  return true;
}

bool gen_eval_ceval(const F* f, size_t max_iterations, double max_seconds, std::ostream& sout, std::ostream& serr) {
  std::chrono::high_resolution_clock timer;
  std::vector<double> x(f->dim());
  double duration;
  std::unique_ptr<fncas::compiled_expression> compiled;
  double compile_time;
  {
    auto e = f->eval_expression(fncas::x(f->dim()));
    auto begin = timer.now();
    compiled = fncas::compile(e);
    auto end = timer.now();
    compile_time =
      std::chrono::duration_cast<std::chrono::duration<int, std::milli>>(end - begin).count() * 0.001;
  }
  size_t iteration = 0;
  auto begin = timer.now();
  do {
    f->gen(x);
    duration =
      std::chrono::duration_cast<std::chrono::duration<int, std::milli>>(timer.now() - begin).count() * 0.001;
    const double golden = f->eval_double(x);
    const double test = compiled->eval(x);
    if (test != golden) {
      serr << golden << " != " << test << " @" << iteration;
      return false;
    }
    ++iteration;
  } while (duration < max_seconds && iteration < max_iterations);
  sout << iteration / duration << ':' << compile_time;
  return true;
}

int main(int argc, char* argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <function> <action> <max_iterations=1000000> <max_seconds=2.0>" << std::endl;
    return -1;
  } else {
    const char* function = argv[1];
    const char* action = argv[2];
    const size_t max_iterations = (argc >= 4) ? atoi(argv[3]) : 1000000;
    const double max_seconds = (argc >= 5) ? atof(argv[4]) : 2.0;
    const F* f = registered_functions[function];
    if (!f) {
      std::cerr << "Function '" << function << "' is not defined in functions/*.h." << std::endl;
      return -1;
    } else {
      typedef boost::function<int(const F*, size_t, double, std::ostream&, std::ostream&)> F_ACTION;
      std::map<std::string, F_ACTION> actions;
      actions["gen"] = gen;
      actions["gen_eval"] = gen_eval;
      actions["gen_eval_ieval"] = gen_eval_ieval;
      actions["gen_eval_ceval"] = gen_eval_ceval;
      F_ACTION action_handler = actions[action];
      if (!action_handler) {
        std::cerr << "Action '" << action << "' is not defined." << std::endl;
        return -1;
      } else {
        std::ostringstream sout;
        std::ostringstream serr;
        sout << std::fixed << std::setprecision(5);
        serr << std::fixed << std::setprecision(5);
        if (action_handler(f, max_iterations, max_seconds, sout, serr)) {
          std::cout << "OK:" << sout.str() << std::endl;
          return 0;
        } else {
          std::cout << "ERROR:" << serr.str() << std::endl;
          std::cerr << serr.str() << std::endl;
          return 1;
        }
      }
    }
  }
}
