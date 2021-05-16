#pragma once

#include "javascript_any.hpp"
#include "javascript_cpp2js.hpp"
#include "javascript_env.hpp"
#include "javascript_js2cpp.hpp"
#include "javascript_object.hpp"

namespace current {
namespace javascript {
namespace impl {

class JSAny final {
 private:
  Napi::Value value_;

 public:
  explicit JSAny(Napi::Value value) : value_(value) {}
  JSAny(const JSObject::JSObjectFieldAccessor value) : value_(value.AsNapiValue()) {}

#ifdef JS_HELPER
#error "Should not `#define JS_HELPER` by this point."
#endif

#define JS_HELPER(T) \
  JSAny(T x) : value_(CPP2JS(x)) {}
  JS_HELPER(int)
  JS_HELPER(double)
  JS_HELPER(bool)
  JS_HELPER(JSObject)
#undef JS_HELPER

  JSAny(const char* s) : value_(CPP2JS(s)) {}
  JSAny(const std::string& s) : value_(CPP2JS(s)) {}  // Construct from a const reference to a string, w/o extra copy.
  JSAny(std::nullptr_t) : value_(CPP2JS(nullptr)) {}

#define JS_HELPER(T) \
  operator T() const { return JS2CPP<T>(value_); }
  JS_HELPER(int)
  JS_HELPER(double)
  JS_HELPER(bool)
  JS_HELPER(JSObject)
  JS_HELPER(std::string)
#undef JS_HELPER

#define JS_HELPER(Kind) \
  bool Is##Kind() const { return value_.Is##Kind(); }
  JS_HELPER(Number)
  JS_HELPER(String)
  JS_HELPER(Boolean)
  JS_HELPER(Object)
  JS_HELPER(Null)
#undef JS_HELPER

  template <typename T>
  T As() const {
    return static_cast<T>(*this);
  }

  operator Napi::Value() const { return value_; }
  operator Napi::Value &() { return value_; }
};

/*
operator JSObject::JSObjectFieldAccessor::JSAny() const {
  const Napi::Value value = object_.Get(key_);
  return JSAny(value);
}
*/

template <>
struct JS2CPPImpl<JSAny> {
  static JSAny DoIt(const Napi::Value& v) { return JSAny(v); }
};

template <>
struct CPP2JSImpl<false, JSAny> {
  using type = Napi::Value;
  static type DoIt(const JSAny& v) { return v; }
  static type DoIt(JSAny&& v) { return std::move(v); }
};

}  // namespace impl
}  // namespace javascript
}  // namespace current
