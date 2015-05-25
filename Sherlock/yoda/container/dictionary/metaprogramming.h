/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
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

#ifndef SHERLOCK_YODA_CONTAINER_DICTIONARY_METAPROGRAMMING_H
#define SHERLOCK_YODA_CONTAINER_DICTIONARY_METAPROGRAMMING_H

#include <utility>

#include "../../metaprogramming.h"
#include "../../../../Bricks/template/pod.h"
#include "../../../../Bricks/template/rmref.h"

namespace yoda {

namespace sfinae {
// TODO(dkorolev): Let's move this to Bricks once we merge repositories?
// Entry key type extractor, getter and setter.
// Supports both `.key` data member and `.key() / .set_key()` methods.
template <typename T_ENTRY>
constexpr bool HasKeyMethod(char) {
  return false;
}

template <typename T_ENTRY>
constexpr auto HasKeyMethod(int) -> decltype(std::declval<T_ENTRY>().key(), bool()) {
  return true;
}

template <typename T_ENTRY, bool HAS_KEY_FUNCTION>
struct KEY_ACCESSOR_IMPL {};

template <typename T_ENTRY>
struct KEY_ACCESSOR_IMPL<T_ENTRY, false> {
  typedef decltype(std::declval<T_ENTRY>().key) T_KEY;
  static bricks::copy_free<T_KEY> GetKey(const T_ENTRY& entry) { return entry.key; }
  static void SetKey(T_ENTRY& entry, T_KEY key) { entry.key = key; }
};

template <typename T_ENTRY>
struct KEY_ACCESSOR_IMPL<T_ENTRY, true> {
  typedef decltype(std::declval<T_ENTRY>().key()) T_KEY;
  // Can not return a reference to a temporary.
  static const T_KEY GetKey(const T_ENTRY& entry) { return entry.key(); }
  static void SetKey(T_ENTRY& entry, bricks::copy_free<T_KEY> key) { entry.set_key(key); }
};

template <typename T_ENTRY>
using KEY_ACCESSOR = KEY_ACCESSOR_IMPL<T_ENTRY, HasKeyMethod<T_ENTRY>(0)>;

template <typename T_ENTRY>
typename KEY_ACCESSOR<T_ENTRY>::T_KEY GetKey(const T_ENTRY& entry) {
  return KEY_ACCESSOR<T_ENTRY>::GetKey(entry);
}

template <typename T_ENTRY>
using ENTRY_KEY_TYPE = bricks::rmconstref<typename KEY_ACCESSOR<T_ENTRY>::T_KEY>;

template <typename T_ENTRY>
void SetKey(T_ENTRY& entry, bricks::copy_free<ENTRY_KEY_TYPE<T_ENTRY>> key) {
  KEY_ACCESSOR<T_ENTRY>::SetKey(entry, key);
}

}  // namespace sfinae
}  // namespace yoda

#endif  // SHERLOCK_YODA_CONTAINER_DICTIONARY_METAPROGRAMMING_H
