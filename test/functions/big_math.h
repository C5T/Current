struct big_math : F {
  enum { DIM = 1000 };
  template<typename T> static typename fncas::output<T>::type f(const T& x) {
    typename fncas::output<T>::type tmp = 0;
    for (size_t i = 0; i < DIM; ++i) {
      tmp += exp(x[i]) + log(x[i]);
    }
    return tmp;
  }
  big_math() {
    for (size_t i = 0; i < DIM; ++i) {
      add_var(boost::exponential_distribution<double>());
    }
  }
};
