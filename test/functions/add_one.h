struct add_one : F {
  virtual const char* name() const {
    return "x + 1";
  }
  template<typename T> typename fncas::output<T>::type f(const T& x) {
    return x[0] + 1;
  }
  add_one() {
    add_var(boost::normal_distribution<double>());
  }
};
