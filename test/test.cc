// TODO(dkorolev): Finish the script to try different compiler/optimization/JIT parameters.
// TODO(dkorolev): Don't forget to actually compare results vs. golden ones, it's a test after all!
//                 This would require resetting the rng, perhaps even changing its type.

#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>

#define FNCAS_JIT nasm
#include "../fncas/fncas.h"

#include "boost/random.hpp"

#include "function.h"
#include "autogen/functions.h"

void generate(const F* f, double seconds) {
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
  std::cout << iterations / duration << std::endl;
}

void evaluate(const F* f, double seconds) {
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
  std::cout << results.size() / duration << std::endl;
}

void intermediate_evaluate(const F* f, double seconds) {
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
  std::cout << results.size() / duration << std::endl;
}

void compiled_evaluate(const F* f, double seconds) {
  std::chrono::high_resolution_clock timer;
  std::vector<double> x(f->dim());
  std::vector<double> results;
  results.reserve(10000000);
  double duration;
  std::unique_ptr<fncas::compiled_expression> e2;
  double compile_time;
  {
    auto e = f->eval_expression(fncas::x(f->dim()));
    auto begin = timer.now();
    e2 = fncas::compile(e);
    auto end = timer.now();
    compile_time =
      std::chrono::duration_cast<std::chrono::duration<int, std::milli>>(end - begin).count() * 0.001;
  }
  auto begin = timer.now();
  do {
    f->gen(x);
    duration =
      std::chrono::duration_cast<std::chrono::duration<int, std::milli>>(timer.now() - begin).count() * 0.001;
    results.push_back(e2->eval(x));
  } while (duration < seconds);
  std::cout << results.size() / duration << ':' << compile_time << std::endl;
}

int main(int argc, char* argv[]) {
  std::cout << std::fixed << std::setprecision(5);
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " <function> <action> <seconds>" << std::endl;
    return -1;
  } else {
    const F* f = registered_functions[argv[1]];
    if (!f) {
      std::cerr << "Function '" << argv[1] << "' is not defined in out functions/*.h." << std::endl;
      return -1;
    } else {
      typedef boost::function<void(const F*, double)> F_ACTION;
      std::map<std::string, F_ACTION> actions;
      actions["generate"] = generate;
      actions["evaluate"] = evaluate;
      actions["intermediate_evaluate"] = intermediate_evaluate;
      actions["compiled_evaluate"] = compiled_evaluate;
      F_ACTION action = actions[argv[2]];
      if (!action) {
        std::cerr << "Action '" << argv[2] << "' is not defined." << std::endl;
        return -1;
      } else {
        action(f, atof(argv[3]));
        return 0;
      }
    }
  }
}
