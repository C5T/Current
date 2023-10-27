/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2023 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2023 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_TYPE_SYSTEM_REFLECTION_TYPEID_H
#define CURRENT_TYPE_SYSTEM_REFLECTION_TYPEID_H

// The most lightweight header to make build times of exported `CURRENT_STRUCT`-s as fast as possible.

// TODO(dkorolev): If I get to extend this further, also export original names in schema-dumped C-structs.

namespace current::reflection {

enum class TypeID : uint64_t;

template <typename T>
struct DefaultCurrentTypeIDImpl;

template <typename T>
struct InjectableCurrentTypeID final {
  static TypeID GetTypeID() {
    return DefaultCurrentTypeIDImpl<T>::GetTypeID();
  }
};

// `CurrentTypeID<T>()` is the "user-facing" type ID of `T`, whereas for each individual `T` the values
// of correponding calls to `InternalCurrentTypeID` may and will be different in case of cyclic dependencies,
// as the order of their resolution by definition depends on which part of the cycle was the starting point.
template <typename T>
reflection::TypeID CurrentTypeID() {
  return InjectableCurrentTypeID<T>::GetTypeID();
}

}  // namespace current::reflection

#define CURRENT_INJECT_TYPE_ID(type,id)      \
namespace current::reflection {              \
template <>                                  \
struct InjectableCurrentTypeID<type> final { \
  static TypeID GetTypeID() {                \
    return static_cast<TypeID>(id);          \
  }                                          \
};                                           \
}

#endif  // CURRENT_TYPE_SYSTEM_REFLECTION_TYPEID_H
