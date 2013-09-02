struct forty_two : F { 
  template<typename T> static typename fncas::output<T>::type f(const T& x) {
    return 42; 
  }
};
