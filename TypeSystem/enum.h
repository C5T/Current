/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef CURRENT_TYPE_SYSTEM_ENUM_H
#define CURRENT_TYPE_SYSTEM_ENUM_H

#include <map>
#include <mutex>
#include <string>
#include <typeindex>

#include "../Bricks/template/enable_if.h"
#include "../Bricks/util/singleton.h"

namespace current {
namespace reflection {

struct EnumNameSingletonImpl {
  template <typename T>
  ENABLE_IF<std::is_enum<T>::value> Register(const char* name) {
    std::lock_guard<std::mutex> lock_guard(mutex_);
    names_[std::type_index(typeid(T))] = name;
  }

  template <typename T>
  ENABLE_IF<std::is_enum<T>::value, std::string> GetName() const {
    const std::type_index type_index = std::type_index(typeid(T));
    if (names_.count(type_index) != 0u) {
      return names_.at(type_index);
    } else {
      // TODO: exception?
      return "";
    }
  }

 private:
  std::map<std::type_index, std::string> names_;
  std::mutex mutex_;
};

template <typename T>
inline ENABLE_IF<std::is_enum<T>::value> RegisterEnum(const char* name) { 
  bricks::Singleton<EnumNameSingletonImpl>().Register<T>(name);
};

template <typename T>
inline ENABLE_IF<std::is_enum<T>::value, std::string> EnumName() {
  return bricks::Singleton<EnumNameSingletonImpl>().GetName<T>();
}

}  // namespace reflection
}  // namespace current

#define CURRENT_REGISTER_ENUM(c) \
    struct CURRENT_ENUM_REGISTERER_##c { \
      CURRENT_ENUM_REGISTERER_##c() { \
        ::current::reflection::RegisterEnum<c>(#c); \
      } \
    }; \
    static CURRENT_ENUM_REGISTERER_##c CURRENT_ENUM_REGISTERER_##c_INSTANCE;    

#endif  // CURRENT_TYPE_SYSTEM_ENUM_H
