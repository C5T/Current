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
  size_t dim() const {
    return p.size();
  }

  virtual double iterations_coefficient() const {
    return 1.0;
  }

  void gen(std::vector<double>& x) const {
    BOOST_ASSERT(x.size() == p.size());
    for (size_t i = 0; i < p.size(); ++i) {
      x[i] = p[i]();
    }
  }

  virtual double eval_double(const std::vector<double>& x) const = 0;
  virtual fncas::output<fncas::x>::type eval_expression(const fncas::x& x) const = 0;
};

std::map<std::string, F*> registered_functions;
template<typename T> void register_function(const char* name, T* impl) {
  registered_functions[name] = impl;
}

#define REGISTER_FUNCTION(F) \
  struct enchanced_##F : F { \
    virtual double eval_double(const std::vector<double>& x) const { \
      return F::f(x); \
    } \
    virtual fncas::output<fncas::x>::type eval_expression(const fncas::x& x) const { \
      return F::f(x); \
    } \
  }; \
  static enchanced_##F F##_impl; \
  static struct F##_registerer { F##_registerer() { register_function<enchanced_##F>(#F, &F##_impl); } } F##_impl_registerer
