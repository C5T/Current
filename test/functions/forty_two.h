struct forty_two : F { 
  virtual const char* name() const {
    return "42";
  }
  template<typename T> typename fncas::output<T>::type f(const T& x) {
    return 42; 
  }
};
