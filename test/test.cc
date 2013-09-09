// TODO: Handle NaN-s properly. Currently it seems that CLANG version eats them while NASM doesn't.

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

// Ah, and if you get an error like this when compiling with clang++:
// /usr/include/c++/4.6/chrono:666:7: error: static_assert expression is not an integral constant expression
// Just comment out the static_assert.

// Normally FNCAS_JIT is set by run_test.sh that runs is with various compilation options.
#ifndef FNCAS_JIT
#error "FNCAS_JIT should be set to build test.cc."
#endif

#include <cassert>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

#include "../fncas/fncas.h"

#include "boost/random.hpp"

#include "function.h"
#include "autogen/functions.h"

bool gen(const F* f, double test_seconds, std::ostream& sout, std::ostream& serr) {
  double duration;
  std::vector<double> x(f->dim());
  uint64_t iteration = 0;
  clock_t begin = clock();
  do {
    f->gen(x);
    duration = double(clock() - begin) / CLOCKS_PER_SEC;
    ++iteration;
  } while (duration < test_seconds);
  sout << iteration / duration;
  return true;
}

bool gen_eval(const F* f, double test_seconds, std::ostream& sout, std::ostream& serr) {
  std::vector<double> x(f->dim());
  double duration;
  uint64_t iteration = 0;
  clock_t begin = clock();
  do {
    f->gen(x);
    f->eval_double(x);
    duration = double(clock() - begin) / CLOCKS_PER_SEC;
    ++iteration;
  } while (duration < test_seconds);
  sout << iteration / duration;
  return true;
}

bool gen_eval_ieval(const F* f, double test_seconds, std::ostream& sout, std::ostream& serr) {
  std::vector<double> x(f->dim());
  double duration;
  auto intermediate = f->eval_expression(fncas::x(f->dim()));
  uint64_t iteration = 0;
  clock_t begin = clock();
  do {
    f->gen(x);
    duration = double(clock() - begin) / CLOCKS_PER_SEC;
    const double golden = f->eval_double(x);
    const double test = intermediate.eval(x);
    if (test != golden) {
      serr << golden << " != " << test << " @" << iteration;
      return false;
    }
    ++iteration;
  } while (duration < test_seconds);
  sout << iteration / duration;
  return true;
}

bool gen_eval_ceval(const F* f, double test_seconds, std::ostream& sout, std::ostream& serr) {
  std::vector<double> x(f->dim());
  double duration;
  std::unique_ptr<fncas::compiled_expression> compiled;
  double compile_time;
  {
    auto e = f->eval_expression(fncas::x(f->dim()));
    clock_t begin = clock();
    compiled = fncas::compile(e);
    compile_time = double(clock() - begin) / CLOCKS_PER_SEC;
  }
  uint64_t iteration = 0;
  clock_t begin = clock();
  do {
    f->gen(x);
    duration = double(clock() - begin) / CLOCKS_PER_SEC;
    const double golden = f->eval_double(x);
    const double test = compiled->eval(x);
    if (test != golden) {
      serr << golden << " != " << test << " @" << iteration;
      return false;
    }
    ++iteration;
  } while (duration < test_seconds);
  sout << iteration / duration << ':' << compile_time;
  return true;
}

int main(int argc, char* argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <function> <action> <test_seconds=60>" << std::endl;
    return -1;
  } else {
    const char* function = argv[1];
    const char* action = argv[2];
    const double test_seconds = (argc >= 4) ? atof(argv[3]) : 60.0;
    const F* f = registered_functions[function];
    if (!f) {
      std::cerr << "Function '" << function << "' is not defined in functions/*.h." << std::endl;
      return -1;
    } else {
      typedef boost::function<int(const F*, double, std::ostream&, std::ostream&)> F_ACTION;
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
        if (action_handler(f, test_seconds, sout, serr)) {
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
