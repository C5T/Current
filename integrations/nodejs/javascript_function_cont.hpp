#pragma once

// NOTE(dkorolev): This has to be a dedicated header file, because of a circular dependency between the other headers,
// as supporting seamless lambda proxying and CPP <=> JavaScript calling requires both `CPP2JS` and `JS2CPP`.

#include "javascript_cpp2js.hpp"
#include "javascript_env.hpp"
#include "javascript_js2cpp.hpp"

namespace current {
namespace javascript {
namespace impl {

template <class T, class ZIPPED_ARGS>
struct CPPFunction2JSIMpl;

template <class LAMBDA>
struct CPPFunction2JSArgsPacker;

template <typename T, typename... ARG_WITHOUT_INDEX>
struct CPPFunction2JSArgsPacker<std::function<T(ARG_WITHOUT_INDEX...)>>
    : CPPFunction2JSIMpl<T, typename ZipArgsWithIndexes<ARG_WITHOUT_INDEX...>::type> {};

template <typename T>
struct LambdaSignatureExtractor : public LambdaSignatureExtractor<decltype(&T::operator())> {};

template <typename ClassType, typename ReturnType, typename... Args>
struct LambdaSignatureExtractor<ReturnType (ClassType::*)(Args...) const> {
  using std_function_t = std::function<ReturnType(Args...)>;
};

template <typename F>
Napi::Function CPPFunction2JS(F&& f) {
  return CPPFunction2JSArgsPacker<typename LambdaSignatureExtractor<F>::std_function_t>::DoIt(std::forward<F>(f));
}

template <typename F>
struct CPP2JSImpl<true, F> {
  using type = Napi::Function;
  static type DoIt(F&& f) { return CPPFunction2JS(std::forward<F>(f)); }
};

// NOTE(dkorolev): Here T` is `ZippedArgWithIndex<>`, which has `::index` and `::type`.
// The very `ZippedArgWithIndex<>` is defined in `javascript_zip_args.hpp`.
template <class T>
typename T::type CallJS2CPPOnIndexedArg(const Napi::CallbackInfo& info) {
  return JS2CPP<typename T::type>(info[T::index]);
}

template <typename T, typename... ARG_WITH_INDEX>
struct CPPFunction2JSIMpl<T, std::tuple<ARG_WITH_INDEX...>> {
  template <typename F>
  static Napi::Function DoIt(F&& f) {
    return Napi::Function::New(JSEnv(), [&f](const Napi::CallbackInfo& info) {
      JSEnvScope scope(info.Env());
      return CPP2JS(f(CallJS2CPPOnIndexedArg<ARG_WITH_INDEX>(info)...));
    });
  }
};

// The specialization of the template defined in `javascript_function.hpp`.
template <class T>
struct JSFunctionCallerImpl<T, true> {
  template <typename... ARGS>
  static T DoItForJSScopedFunctionReturning(const Napi::Function& function, ARGS&&... args) {
    Napi::Value v = function.Call({CPP2JS(std::forward<ARGS>(args))...});
    return JS2CPP<T>(v);
  }
  template <typename... ARGS>
  static T DoItForJSFunctionReturning(const Napi::FunctionReference& function_reference,
                                      const Napi::AsyncContext& async_context,
                                      ARGS&&... args) {
    return JS2CPP<T>(
        function_reference.MakeCallback(JSEnv().Global(), {CPP2JS(std::forward<ARGS>(args))...}, async_context));
  }
};

}  // namespace current::javascript::impl
}  // namespace current::javascript
}  // namespace current
