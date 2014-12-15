#ifndef BRICKS_JAVA_WRAPPER_H
#define BRICKS_JAVA_WRAPPER_H

#include "../port.h"

#if !defined(BRICKS_JAVA) && !defined(BRICKS_ANDROID)
#error "`java_wrapper.h` should only be compiled on Java and Android."
#endif

#include <jni.h>
#include <string>

// TODO(dkorolev): Make this work using a templated reinterpret_cast<>.

#ifndef ANDROID
#define PLATFORM_SPECIFIC_CAST (void**)
#else
#define PLATFORM_SPECIFIC_CAST
#endif

#define CLEAR_AND_RETURN_FALSE_ON_EXCEPTION \
  if (env->ExceptionCheck()) {              \
    env->ExceptionDescribe();               \
    env->ExceptionClear();                  \
    return false;                           \
  }

#define RETURN_ON_EXCEPTION    \
  if (env->ExceptionCheck()) { \
    env->ExceptionDescribe();  \
    return;                    \
  }

namespace bricks {
namespace java_wrapper {

struct JavaWrapper {
  struct Instance {
    JavaVM* jvm = nullptr;
    jclass httpParamsClass = 0;
    jmethodID httpParamsConstructor = 0;
    jclass httpTransportClass = 0;
    jmethodID httpTransportClass_run = 0;
  };

  static Instance& Singleton() {
    static Instance instance;
    return instance;
  }
};

inline std::string ToStdString(JNIEnv* env, jstring str) {
  std::string result;
  char const* utfBuffer = env->GetStringUTFChars(str, 0);
  if (utfBuffer) {
    result = utfBuffer;
    env->ReleaseStringUTFChars(str, utfBuffer);
  }
  return result;
}

}  // namespace java_wrapper
}  // namespace bricks

#ifdef LINK_JAVA_ON_LOAD_INTO_SOURCE
#include "java_wrapper.cc"
#endif  // LINK_JAVA_ON_LOAD_INTO_SOURCE

#endif  // BRICKS_JAVA_WRAPPER_H
