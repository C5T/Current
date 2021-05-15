#pragma once

#include "javascript_cpp2js.hpp"
#include "javascript_env.hpp"
#include "javascript_js2cpp.hpp"

namespace current {
namespace javascript {
namespace impl {

class JSObject final {
 private:
  Napi::Object object_;

 public:
  explicit JSObject() : object_(Napi::Object::New(JSEnv())) {}
  explicit JSObject(Napi::Object object) : object_(object) {}

  class JSObjectFieldAccessor final {
   private:
    Napi::Object& object_;
    const std::string key_as_string_;
    const char* key_;

   public:
    JSObjectFieldAccessor(Napi::Object& object, const char* key) : object_(object), key_(key) {}
    JSObjectFieldAccessor(Napi::Object& object, std::string key)
        : object_(object), key_as_string_(std::move(key)), key_(key_as_string_.c_str()) {}

    template <typename T>
    void operator=(T&& value) {
      object_[key_] = CPP2JS(std::forward<T>(value));
    }

#ifdef JS_HELPER
#error "Should not `#define JS_HELPER` by this point."
#endif

#define JS_HELPER(T) \
  operator T() const { return JS2CPP<T>(object_[key_]); }

    JS_HELPER(int)
    JS_HELPER(double)
    JS_HELPER(bool)
    JS_HELPER(std::string)

#undef JS_HELPER
  };

  operator Napi::Object() const { return object_; }

  JSObjectFieldAccessor operator[](const char* key) { return JSObjectFieldAccessor(object_, key); }
  JSObjectFieldAccessor operator[](std::string key) { return JSObjectFieldAccessor(object_, std::move(key)); }

  template <typename T>
  JSObject& Add(const std::string& key, T&& value) {
    object_[key] = CPP2JS(std::forward<T>(value));
    return *this;
  }

  template <typename T>
  JSObject& Add(const char* key, T&& value) {
    object_[key] = CPP2JS(std::forward<T>(value));
    return *this;
  }
};

template <>
struct JS2CPPImpl<JSObject> {
  static JSObject DoIt(const Napi::Value& v) { return JSObject(v.As<Napi::Object>()); }
};

template <>
struct CPP2JSImpl<false, JSObject> {
  using type = Napi::Object;
  static type DoIt(const JSObject& o) { return o; }
  static type DoIt(JSObject&& o) { return std::move(o); }
};

}  // namespace impl
}  // namespace javascript
}  // namespace current
