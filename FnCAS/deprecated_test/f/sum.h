struct sum : F {
  INCLUDE_IN_SMOKE_TEST;
  enum { DIM = 10 };
  template <typename X>
  static X2V<X> f(const X& x) {
    X2V<X> r = 0;
    for (size_t i = 0; i < DIM; ++i) {
      r += x[i];
    }
    return r;
  }
  std::normal_distribution<double> distribution_;
  sum() {
    for (size_t i = 0; i < DIM; ++i) {
      add_var(distribution_);
    }
  }
};
