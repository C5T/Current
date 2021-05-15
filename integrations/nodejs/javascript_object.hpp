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
    Napi::ObjectReference object_;
    const std::string key_as_string_;
    const char* key_;

   protected:
    friend class JSObject;
    JSObjectFieldAccessor(Napi::ObjectReference object, const char* key) : object_(std::move(object)), key_(key) {}
    JSObjectFieldAccessor(Napi::ObjectReference object, std::string key)
        : object_(std::move(object)), key_as_string_(std::move(key)), key_(key_as_string_.c_str()) {}

   public:
    template <typename T>
    void operator=(T&& value) {
      object_.Set(key_, CPP2JS(std::forward<T>(value)));
    }

#ifdef JS_HELPER
#error "Should not `#define JS_HELPER` by this point."
#endif

#define JS_HELPER(T) \
  operator T() const { return JS2CPP<T>(object_.Get(key_)); }

    JS_HELPER(int)
    JS_HELPER(double)
    JS_HELPER(bool)
    JS_HELPER(std::string)

#undef JS_HELPER

    operator JSObject() const {
      // NOTE(dkorolev): Would fail if the key already exists and its value itself is not an object.
      Napi::Value value = object_.Get(key_);
      return JSObject(value.As<Napi::Object>());
    }

    operator JSObject() {
      if (!object_.Value().Has(key_)) {
        // NOTE(dkorolev): Much like in C++, `operator[]` on a `JSObject` creates a new key.
        //                 This is to support the `a["several"]["new"]["keys"] = true` synopsis.
        Napi::Object nested_object = Napi::Object::New(JSEnv());
        object_.Set(key_, nested_object);
        return JSObject(nested_object);  // ForwardDeclareJS2CPPImplOnObject(nested_object);
      } else {
        // NOTE(dkorolev): Would fail if the key already exists and its value itself is not an object.
        Napi::Value value = object_.Get(key_);
        return JSObject(value.As<Napi::Object>());
      }
    }

    template <typename T>
    T As() const {
      return static_cast<T>(*this);
    }

    JSObjectFieldAccessor operator[](const char* key) {
      if (!object_.Value().Has(key_)) {
        // NOTE(dkorolev): Much like in C++, `operator[]` on a `JSObject` creates a new key.
        //                 This is to (eventually) support the `a["several"]["new"]["keys"] = true` synopsis.
        Napi::Object nested_object = Napi::Object::New(JSEnv());
        object_.Set(key_, nested_object);
        return JSObjectFieldAccessor(Napi::ObjectReference::New(nested_object), key);
      } else {
        // NOTE(dkorolev): Would fail if the key already exists and its value itself is not an object.
        Napi::Value value = object_.Get(key_);
        return JSObjectFieldAccessor(Napi::ObjectReference::New(value.As<Napi::Object>()), key);
      }
    }
  };

  operator Napi::Object() const { return object_; }

  JSObjectFieldAccessor operator[](const char* key) {
    return JSObjectFieldAccessor(Napi::ObjectReference::New(object_), key);
  }
  JSObjectFieldAccessor operator[](std::string key) {
    return JSObjectFieldAccessor(Napi::ObjectReference::New(object_), std::move(key));
  }

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
