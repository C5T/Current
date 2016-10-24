/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Alexander Zolotarev <me@alex.bio> from Minsk, Belarus
          (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef BRICKS_UTIL_SINGLETON_H
#define BRICKS_UTIL_SINGLETON_H

#include "../port.h"

#ifndef CURRENT_HAS_THREAD_LOCAL
#include <pthread.h>  // To emulate `thread_local` via `pthread_*`.
#include <cstdlib>
#include <iostream>
#endif

namespace current {

template <typename T>
inline T& Singleton() {
  static T instance;
  return instance;
}

#ifdef CURRENT_HAS_THREAD_LOCAL
template <typename T>
inline T& ThreadLocalSingleton() {
  thread_local static T instance;
  return instance;
}
#else
template <typename T>
class ThreadLocalSingletonImpl final {
 private:
  static pthread_key_t& pthread_key_t_static_storage() {
    static pthread_key_t key;
    return key;
  }

  static pthread_once_t& pthread_once_t_static_storage() {
    static pthread_once_t key_is_initialized = PTHREAD_ONCE_INIT;
    return key_is_initialized;
  }

  static void Deleter(void* ptr) { delete reinterpret_cast<T*>(ptr); }

  static void CreateKey() {
    if (pthread_key_create(&(ThreadLocalSingletonImpl::pthread_key_t_static_storage()),
                           ThreadLocalSingletonImpl::Deleter)) {
      std::cerr << "Error in `pthread_key_create()`. Terminating." << std::endl;
      std::exit(EXIT_FAILURE);
    }
  }

  static pthread_key_t& GetKey() {
    if (pthread_once(&(ThreadLocalSingletonImpl::pthread_once_t_static_storage()),
                     ThreadLocalSingletonImpl::CreateKey)) {
      std::cerr << "Error in `pthread_once()`. Terminating." << std::endl;
      std::exit(EXIT_FAILURE);
    }
    return ThreadLocalSingletonImpl::pthread_key_t_static_storage();
  }

 public:
  static T& GetInstance() {
    pthread_key_t& key = ThreadLocalSingletonImpl::GetKey();
    T* ptr = reinterpret_cast<T*>(pthread_getspecific(key));
    if (!ptr) {
      ptr = new T();
      pthread_setspecific(key, ptr);
    }
    return *ptr;
  }
};

template <typename T>
inline T& ThreadLocalSingleton() {
  return ThreadLocalSingletonImpl<T>::GetInstance();
}
#endif

}  // namespace current

#endif  // BRICKS_UTIL_SINGLETON_H
