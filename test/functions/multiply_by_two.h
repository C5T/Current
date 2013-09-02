struct multiply_by_two : F {
  template<typename T> static typename fncas::output<T>::type f(const T& x) {
    return x[0] * 2;
  }
  multiply_by_two() {
    add_var(boost::normal_distribution<double>());
  }
};
