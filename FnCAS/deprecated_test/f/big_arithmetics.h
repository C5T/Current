struct big_arithmetics : F {
  INCLUDE_IN_PERF_TEST;
  enum { DIM = 100000 };
  template <typename T>
  static T f(const std::vector<T>& x) {
    T r = 0.0;
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
  big_arithmetics() : distribution_(1.0) {
    for (size_t i = 0; i < DIM; ++i) {
      add_var(distribution_);
    }
  }
};
