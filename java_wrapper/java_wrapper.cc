#include "java_wrapper.h"

#include <jni.h>

extern "C" {
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
  auto& JAVA = bricks::java_wrapper::JavaWrapper::Singleton();
  JAVA.jvm = vm;
  return JNI_VERSION_1_6;
}
}  // extern "C"
