struct arithmetics : F {
  INCLUDE_IN_SMOKE_TEST;
  enum { DIM = 100 };
  template <typename T> static typename fncas::output<T>::type f(const T& x) {
    typename fncas::output<T>::type r = 0;
    for (size_t i = 0; i < DIM; ++i) {
      switch (i % 4) {
        case 0:
          r += x[i];
          break;
        case 1:
          r -= x[i];
          break;
        case 2:
          r *= x[i];
          break;
        case 3:
          r /= x[i];
          break;
      }
    }
    return r;
  }
  arithmetics() {
    for (size_t i = 0; i < DIM; ++i) {
      add_var(boost::exponential_distribution<double>());
    }
  }
};
