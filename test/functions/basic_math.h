struct basic_math : F {
  virtual const char* name() const {
    return "exp(a) + log(b) + sin(c) + cos(d) + tan(e) + atan(f)";
  }
  template<typename T> typename fncas::output<T>::type f(const T& x) {
    return exp(x[0]) + log(x[1]) + sin(x[2]) + cos(x[3]) + tan(x[4]) + atan(x[5]);
  }
  basic_math() {
    add_var(boost::normal_distribution<double>());
    add_var(boost::exponential_distribution<double>());
    add_var(boost::normal_distribution<double>());
    add_var(boost::normal_distribution<double>());
    add_var(boost::normal_distribution<double>());
    add_var(boost::normal_distribution<double>());
  }
};
