#pragma once

#include "javascript_function.hpp"

namespace current {
namespace javascript {
namespace impl {

template <typename T>
struct JS2CPPImpl;

template <typename T>
T JS2CPP(const Napi::Value& v) {
  return JS2CPPImpl<T>::DoIt(v);
}

template <>
struct JS2CPPImpl<int> {
  static int DoIt(const Napi::Value& v) { return static_cast<int>(v.As<Napi::Number>()); }
};

template <>
struct JS2CPPImpl<double> {
  static double DoIt(const Napi::Value& v) { return static_cast<double>(v.As<Napi::Number>()); }
};

template <>
struct JS2CPPImpl<bool> {
  static bool DoIt(const Napi::Value& v) { return static_cast<bool>(v.As<Napi::Boolean>()); }
};

template <>
struct JS2CPPImpl<std::string> {
  static std::string DoIt(const Napi::Value& v) { return v.As<Napi::String>(); }
};

template <typename T>
struct JS2CPPImpl<JSFunctionReferenceReturning<T>> {
  static JSFunctionReferenceReturning<T> DoIt(const Napi::Value& v) {
    return JSFunctionReferenceReturning<T>(v.As<Napi::Function>());
  }
};

template <typename T>
struct JS2CPPImpl<JSFunctionReturning<T>> {
  static JSFunctionReturning<T> DoIt(const Napi::Value& v) { return JSFunctionReturning<T>(v.As<Napi::Function>()); }
};

}  // namespace impl
}  // namespace javascript
}  // namespace current
