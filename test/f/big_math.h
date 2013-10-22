struct big_math : F {
  INCLUDE_IN_PERF_TEST;
  enum { DIM = 100000 };
  template<typename T> static typename fncas::output<T>::type f(const T& x) {
    typename fncas::output<T>::type r = 0.0;
    for (size_t i = 0; i < DIM; ++i) {
      switch (i % 6) {
        case 0: r += exp(x[i] * 0.01); break;
        case 1: r += log(x[i]*x[i] + 1.0); break;
        case 2: r += sin(x[i]); break;
        case 3: r += cos(x[i]); break;
        case 4: r += tan(x[i] * 0.01); break;
        case 5: r += atan(x[i] * 0.01); break;
      }
    }
    return r;
  }
  big_math() {
    for (size_t i = 0; i < DIM; ++i) {
      add_var(boost::normal_distribution<double>());
    }
  }
};
