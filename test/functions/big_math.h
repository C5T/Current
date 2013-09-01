struct big_math : F {
  enum { DIM = 1000 };
  virtual std::string name() const {
    std::ostringstream os;
    os << "sum<i=[0.." << DIM << ")>(exp(x[i]) + log(x[i]))";
    return os.str();
  }
  template<typename T> typename fncas::output<T>::type f(const T& x) {
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
  virtual double iterations_coefficient() const { return 1; }
};
