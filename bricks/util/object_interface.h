/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_UTIL_OBJECT_INTERFACE_H
#define BRICKS_UTIL_OBJECT_INTERFACE_H

#define CURRENT_OBJECT_INTERFACE(type)                          \
  template <class>                                              \
  struct CURRENT_OBJECT_INTERFACE_METHODS_WRAPPER;              \
  template <class>                                              \
  class CURRENT_OBJECT_INTERFACE_IMPL_POINTER_CONTAINER;        \
  template <>                                                   \
  class CURRENT_OBJECT_INTERFACE_IMPL_POINTER_CONTAINER<type> { \
   public:                                                      \
    using CURRENT_IMPLEMENTATION_POINTER_TYPE = type;           \
                                                                \
   protected:                                                   \
    type* CURRENT_IMPLEMENTATION_POINTER;                       \
  };                                                            \
  template <>                                                   \
  struct CURRENT_OBJECT_INTERFACE_METHODS_WRAPPER<type> : CURRENT_OBJECT_INTERFACE_IMPL_POINTER_CONTAINER<type>

#define CURRENT_FORWARD_METHOD(method)                                                                         \
  template <typename... ARGS>                                                                                  \
  auto method(ARGS&&... args) {                                                                                     \
    return this->CURRENT_IMPLEMENTATION_POINTER->method(std::forward<ARGS>(args)...);                          \
  }

#endif  // BRICKS_UTIL_OBJECT_INTERFACE_H
