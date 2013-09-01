struct deep_function_tree : F {
  enum { DIM = 100000 };
  virtual std::string name() const {
    std::ostringstream os;
    os << "sum<i=[0.." << DIM << ")>(x[i])";
    return os.str();
  }
  template<typename T> typename fncas::output<T>::type f(const T& x) {
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
  virtual double iterations_coefficient() const { return 0.5; }
};
