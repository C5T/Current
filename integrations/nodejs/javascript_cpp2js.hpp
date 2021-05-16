#pragma once

#include "javascript_env.hpp"
#include "javascript_zip_args.hpp"

namespace current {
namespace javascript {
namespace impl {

template <typename T>
class IsCallable final {
 private:
  using One = char;
  using Two = int;
  static_assert(sizeof(One) != sizeof(Two), "FATAL.");

  template <typename TEST>
  static One Check(decltype(&TEST::operator()));

  template <typename>
  static Two Check(...);

 public:
  enum { value = sizeof(Check<T>(0)) == sizeof(One) };
};

template <typename R, typename... A>
class IsCallable<std::function<R(A...)>> final {
 public:
  enum { value = true };
};

template <bool CALLABLE, typename T>
struct CPP2JSImpl;

template <typename T>
typename CPP2JSImpl<IsCallable<typename std::decay<T>::type>::value, typename std::decay<T>::type>::type CPP2JS(T&& x) {
  return CPP2JSImpl<IsCallable<typename std::decay<T>::type>::value, typename std::decay<T>::type>::DoIt(
      std::forward<T>(x));
}

template <>
struct CPP2JSImpl<false, int> {
  using type = Napi::Number;
  static type DoIt(int x) { return type::New(JSEnv(), x); }
};

template <>
struct CPP2JSImpl<false, double> {
  using type = Napi::Number;
  static type DoIt(double x) { return type::New(JSEnv(), x); }
};

template <>
struct CPP2JSImpl<false, const char*> {
  using type = Napi::String;
  static type DoIt(const char* x) { return type::New(JSEnv(), x); }
};

template <>
struct CPP2JSImpl<false, char*> {
  using type = Napi::String;
  static type DoIt(const char* x) { return type::New(JSEnv(), x); }
};

template <size_t N>
struct CPP2JSImpl<false, const char[N]> {
  using type = Napi::String;
  static type DoIt(const char* x) { return type::New(JSEnv(), x); }
};

template <size_t N>
struct CPP2JSImpl<false, char[N]> {
  using type = Napi::String;
  static type DoIt(const char* x) { return type::New(JSEnv(), x); }
};

template <>
struct CPP2JSImpl<false, std::string> {
  using type = Napi::String;
  static type DoIt(const std::string& s) { return type::New(JSEnv(), s); }
  static type DoIt(std::string&& s) { return type::New(JSEnv(), std::move(s)); }
};

template <>
struct CPP2JSImpl<false, bool> {
  using type = Napi::Boolean;
  static type DoIt(bool x) { return type::New(JSEnv(), x); }
};

template <>
struct CPP2JSImpl<false, std::nullptr_t> {
  using type = Napi::Value;
  static type DoIt(std::nullptr_t) { return JSEnv().Null(); }
};

struct Undefined final {};

template <>
struct CPP2JSImpl<false, Undefined> {
  using type = Napi::Value;
  static type DoIt(Undefined) { return JSEnv().Undefined(); }
};

// NOTE(dkorolev): See also `javascript_function_cont.hpp` for `CPP2JS` support for lambdas.

}  // namespace impl
}  // namespace javascript
}  // namespace current
