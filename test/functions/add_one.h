struct add_one : F {
  template<typename T> static typename fncas::output<T>::type f(const T& x) {
    return x[0] + 1;
  }
  add_one() {
    add_var(boost::normal_distribution<double>());
  }
};
