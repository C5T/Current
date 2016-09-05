/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_SERIALIZATION_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_SERIALIZATION_H

#include "exceptions.h"

#include "../../Bricks/template/decay.h"

namespace current {
namespace serialization {

template <class SERIALIZER, typename T, typename ENABLE = void>
struct SerializeImpl;

template <class SERIALIZER, typename T, typename ENABLE = void>
struct DeserializeImpl;

template <class SERIALIZER, typename T>
inline void Serialize(SERIALIZER&& serializer, T&& x) {
  SerializeImpl<current::decay<SERIALIZER>, current::decay<T>>::DoSerialize(
      std::forward<SERIALIZER>(serializer), std::forward<T>(x));
}

template <class DESERIALIZER, typename T>
inline void Deserialize(DESERIALIZER&& deserializer, T& x) {
  DeserializeImpl<current::decay<DESERIALIZER>, T>::DoDeserialize(std::forward<DESERIALIZER>(deserializer), x);
}

}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_SERIALIZATION_H
