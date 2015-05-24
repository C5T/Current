struct arithmetics : F {
  INCLUDE_IN_SMOKE_TEST;
  enum { DIM = 100 };
  template <typename X>
  static X2V<X> f(const X& x) {
    X2V<X> r = 0;
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
  std::exponential_distribution<double> distribution_;
  arithmetics() : distribution_(1.0) {
    for (size_t i = 0; i < DIM; ++i) {
      add_var(distribution_);
    }
  }
};
