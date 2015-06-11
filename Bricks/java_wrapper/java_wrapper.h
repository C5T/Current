/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Alexander Zolotarev <me@alex.bio> from Minsk, Belarus
          (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

#ifndef BRICKS_JAVA_WRAPPER_H
#define BRICKS_JAVA_WRAPPER_H

#include "../port.h"

#if defined(BRICKS_JAVA) || defined(BRICKS_ANDROID)

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

#else  // defined(BRICKS_JAVA) || defined(BRICKS_ANDROID)
#ifndef CURRENT_TEST_COMPILATION
#error "`java_wrapper.h` should only be compiled on Java and Android."
#endif  // CURRENT_TEST_COMPILATION

#endif  // defined(BRICKS_JAVA) || defined(BRICKS_ANDROID)


#endif  // BRICKS_JAVA_WRAPPER_H
