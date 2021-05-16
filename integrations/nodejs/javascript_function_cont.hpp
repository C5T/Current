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
struct CPPFunction2JSImpl;

template <class LAMBDA>
struct CPPFunction2JSArgsPacker;

template <typename T, typename... ARG_WITHOUT_INDEX>
struct CPPFunction2JSArgsPacker<std::function<T(ARG_WITHOUT_INDEX...)>>
    : CPPFunction2JSImpl<T, typename ZipArgsWithIndexes<ARG_WITHOUT_INDEX...>::type> {};

template <typename T>
struct LambdaSignatureExtractorImpl;

template <typename T>
struct LambdaSignatureExtractor {
  using std_function_t = typename LambdaSignatureExtractorImpl<decltype(&T::operator())>::std_function_t;
};

template <typename R, typename... ARGS>
struct LambdaSignatureExtractor<std::function<R(ARGS...)>> {
  using std_function_t = std::function<R(ARGS...)>;
};

template <typename T, typename R, typename... ARGS>
struct LambdaSignatureExtractorImpl<R (T::*)(ARGS...)> {
  using std_function_t = std::function<R(ARGS...)>;
};

template <typename T, typename R, typename... ARGS>
struct LambdaSignatureExtractorImpl<R (T::*)(ARGS...) const> {
  using std_function_t = std::function<R(ARGS...)>;
};

template <typename F>
Napi::Function CPPFunction2JS(F&& f) {
  return CPPFunction2JSArgsPacker<
      typename LambdaSignatureExtractor<typename std::decay<F>::type>::std_function_t>::DoIt(std::forward<F>(f));
}

template <typename F>
struct CPP2JSImpl<true, F> {
  using type = Napi::Function;
  template <typename FF>
  static type DoIt(FF&& f) {
    return CPPFunction2JS(std::forward<FF>(f));
  }
};

// NOTE(dkorolev): Here T` is `ZippedArgWithIndex<>`, which has `::index` and `::type`.
// The very `ZippedArgWithIndex<>` is defined in `javascript_zip_args.hpp`.
template <class T>
typename T::type CallJS2CPPOnIndexedArg(const Napi::CallbackInfo& info) {
  return JS2CPP<typename T::type>(info[T::index]);
}

// NOTE(dkorolev): I have confirmed the `CopyableHelper` + `SharedPtrContents` combo does the job with respect
// to not deleting the user data associated with the function. I have *not* confirmed that
// the very `SharedPtrContents` is removed once the JavaScript function is being garbage-collected.
template <typename T>
struct SharedPtrContents final {
  T contents;

  SharedPtrContents(const T& cpp) : contents(cpp) {}
  SharedPtrContents(T&& cpp) : contents(cpp) {}

  SharedPtrContents() = delete;
  SharedPtrContents& operator=(const SharedPtrContents&) = delete;
  SharedPtrContents& operator=(SharedPtrContents&&) = delete;
};

template <typename RETVAL, class F, class... ARG_WITH_INDEX>
class CopyableHelper final {
 private:
  std::shared_ptr<SharedPtrContents<F>> shared_copyable_funciton;

 public:
  CopyableHelper() = delete;

  CopyableHelper(F&& f) : shared_copyable_funciton(std::make_shared<SharedPtrContents<F>>(std::move(f))) {}
  CopyableHelper(const F& f) : shared_copyable_funciton(std::make_shared<SharedPtrContents<F>>(f)) {}

  CopyableHelper(const CopyableHelper&) = default;
  CopyableHelper(CopyableHelper&&) = default;
  CopyableHelper& operator=(const CopyableHelper&) = default;
  CopyableHelper& operator=(CopyableHelper&&) = default;

  template <typename R = RETVAL>
  typename std::enable_if<!std::is_same<R, void>::value, Napi::Value>::type operator()(const Napi::CallbackInfo& info) {
    JSEnvScope scope(info.Env());
    return CPP2JS(shared_copyable_funciton->contents(CallJS2CPPOnIndexedArg<ARG_WITH_INDEX>(info)...));
  }

  template <typename R = RETVAL>
  typename std::enable_if<std::is_same<R, void>::value, Napi::Value>::type operator()(const Napi::CallbackInfo& info) {
    JSEnvScope scope(info.Env());
    shared_copyable_funciton->contents(CallJS2CPPOnIndexedArg<ARG_WITH_INDEX>(info)...);
    return CPP2JS(Undefined());
  }
};

template <typename T, typename... ARG_WITH_INDEX>
struct CPPFunction2JSImpl<T, std::tuple<ARG_WITH_INDEX...>> {
  template <typename F>
  static Napi::Function DoIt(F&& f) {
    return Napi::Function::New(JSEnv(),
                               CopyableHelper<T, typename std::decay<F>::type, ARG_WITH_INDEX...>(std::forward<F>(f)));
  }
};

// The specialization of the template defined in `javascript_function.hpp`.
// NOTE(dkorolev): Effectively, this template and its specialization are the forward declaration.
template <class T>
struct JSFunctionCallerImpl<T, true> {
  template <typename... ARGS>
  static T DoItForJSFunctionReferenceReturning(const Napi::Function& function, ARGS&&... args) {
    // NOTE(dkorolev): Unused as of now, as I've moved every call to use `FunctionReference`.
    Napi::Value v = function.Call({CPP2JS(std::forward<ARGS>(args))...});
    return JS2CPP<T>(v);
  }

  template <typename... ARGS>
  static T DoItForJSFunctionReferenceReturning(const Napi::FunctionReference& function, ARGS&&... args) {
    Napi::Value v = function.MakeCallback(JSEnv().Global(), {CPP2JS(std::forward<ARGS>(args))...});
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

}  // namespace impl
}  // namespace javascript
}  // namespace current
