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

  std::vector<double> gen() const {
    std::vector<double> x(p.size());
    for (size_t i = 0; i < p.size(); ++i) {
      x[i] = p[i]();
    }
    return x;
  }
};
