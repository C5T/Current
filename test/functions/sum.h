struct sum : F {
  enum { DIM = 100000 };
  template<typename T> static typename fncas::output<T>::type f(const T& x) {
    typename fncas::output<T>::type r = 0;
    for (size_t i = 0; i < DIM; ++i) {
      r += x[i];
    }
    return r;
  }
  sum() {
    for (size_t i = 0; i < DIM; ++i) {
      add_var(boost::normal_distribution<double>());
    }
  }
};
