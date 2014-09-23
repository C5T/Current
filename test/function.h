// Defines abstract function definition for all f/*.h.
// The functions to be tested should be derived from this class.
// Check out any f/*.h, like f/sum.h, for an example.
//
// TODO(dkorolev): Implement assembly-based code generation and test it.
//
// Functions implemented in f/*.h also have some "tags" specified. Current tags are:
//
// * INCLUDE_IN_SMOKE_TEST: This function is to be included in the smoke test.
//   The smoke test would generate multiple random inputs and test that
//   the value of the function is equal across various ways to compute it.
//   TODO(dkorolev): Smoke test should also test the derivatives.
//   TODO(dkorolev): Add composite functions to the smoke test.
//
// * INCLUDE_IN_PERF_TEST: This function is to be included in the perf test.
//   Perf test would compute the QPS for this function takes to compute
//   under various circumstances and produce an HTML report on the performance
//   of various methods to compute the function:
//   { native, interpreted, compiled with different compilers, etc. }.
//   Perf test doesn't make sense for tiny functions, and, on the other hand,
//   it doesn't make sense to include large functions in the smoke test,
//   hence the tags.
//   Naturally, perf test also verifies that computation results match,
//   so it also serves as a larger-scale smoke test.
//   TODO(dkorolev): Add a few composite functions to the perf test as well.

class F {
 private:
  boost::mt19937 rng;
  std::vector<boost::function<double()>> p;

 protected:
  template <typename T> void add_var(T var) {
    p.push_back(boost::variate_generator<boost::mt19937&, T>(rng, var));
  }

 public:
  size_t dim() const {
    return p.size();
  }

  void gen(std::vector<double>& x) const {
    BOOST_ASSERT(x.size() == p.size());
    for (size_t i = 0; i < p.size(); ++i) {
      x[i] = p[i]();
    }
  }

  // To support registration macros.
  virtual double eval_as_double(const std::vector<double>& x) const = 0;
  virtual fncas::output<fncas::x>::type eval_as_expression(const fncas::x& x) const = 0;
};

std::map<std::string, F*> registered_functions;
template <typename T> void register_function(const char* name, T* impl) {
  registered_functions[name] = impl;
}

// To support registration macros.
#define REGISTER_FUNCTION(F)                                                            \
  struct enhanced_##F : F {                                                             \
    virtual double eval_as_double(const std::vector<double>& x) const {                 \
      return F::f(x);                                                                   \
    }                                                                                   \
    virtual fncas::output<fncas::x>::type eval_as_expression(const fncas::x& x) const { \
      return F::f(x);                                                                   \
    }                                                                                   \
  };                                                                                    \
  static enhanced_##F F##_impl;                                                         \
  static struct F##_registerer {                                                        \
    F##_registerer() {                                                                  \
      register_function<enhanced_##F>(#F, &F##_impl);                                   \
    }                                                                                   \
  } F##_impl_registerer

#define INCLUDE_IN_SMOKE_TEST const bool INCLUDE_IN_SMOKE_TEST_ = true
#define INCLUDE_IN_PERF_TEST const bool INCLUDE_IN_PERF_TEST_ = true
