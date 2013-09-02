struct deep_function_tree : F {
  enum { DIM = 100000 };
  template<typename T> static typename fncas::output<T>::type f(const T& x) {
    typename fncas::output<T>::type tmp = 42;
    for (size_t i = 0; i < DIM; ++i) {
      tmp += x[i];
    }
    return tmp;
  }
  deep_function_tree() {
    for (size_t i = 0; i < DIM; ++i) {
      add_var(boost::normal_distribution<double>());
    }
  }
};
