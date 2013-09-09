struct math : F {
  enum { DIM = 10000 };
  template<typename T> static typename fncas::output<T>::type f(const T& x) {
    typename fncas::output<T>::type r = 0.0;
    for (size_t i = 0; i < DIM; ++i) {
      switch (i % 6) {
        case 0: r += exp(x[i]); break;
        case 1: r += log(x[i]); break;
        case 2: r += sin(x[i]); break;
        case 3: r += cos(x[i]); break;
        case 4: r += tan(x[i]); break;
        case 5: r += atan(x[i]); break;
      }
    }
    return r;
  }
  math() {
    for (size_t i = 0; i < DIM; ++i) {
      add_var(boost::exponential_distribution<double>());
    }
  }
};
