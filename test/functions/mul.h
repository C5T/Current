struct mul : F {
  enum { DIM = 100000 };
  template<typename T> static typename fncas::output<T>::type f(const T& x) {
    typename fncas::output<T>::type r = 1;
    for (size_t i = 0; i < DIM; ++i) {
      r *= x[i];
    }
    return r;
  }
  mul() {
    for (size_t i = 0; i < DIM; ++i) {
      add_var(boost::exponential_distribution<double>());
    }
  }
};
