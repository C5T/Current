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
