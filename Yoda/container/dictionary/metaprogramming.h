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

#include "../../types.h"
#include "../../metaprogramming.h"

#include "../../../Bricks/template/pod.h"
#include "../../../Bricks/template/decay.h"

namespace yoda {

namespace sfinae {

template <typename T>
using CF = current::copy_free<T>;

// TODO(dkorolev): Let's move this to Bricks once we merge repositories?
// Entry key type extractor, getter and setter.
// Supports both `.key` data member and `.key() / .set_key()` methods.
template <typename ENTRY>
constexpr bool HasKeyMethod(char) {
  return false;
}

template <typename ENTRY>
constexpr auto HasKeyMethod(int) -> decltype(std::declval<const ENTRY>().key(), bool()) {
  return true;
}

template <typename ENTRY, bool HAS_KEY_FUNCTION>
struct KEY_ACCESSOR_IMPL {};

template <typename ENTRY>
struct KEY_ACCESSOR_IMPL<ENTRY, false> {
  typedef decltype(std::declval<ENTRY>().key) T_KEY;
  static CF<T_KEY> GetKey(const ENTRY& entry) { return entry.key; }
  static void SetKey(ENTRY& entry, T_KEY key) { entry.key = key; }
};

template <typename ENTRY>
struct KEY_ACCESSOR_IMPL<ENTRY, true> {
  typedef decltype(std::declval<ENTRY>().key()) T_KEY;
  // Can not return a reference to a temporary.
  static const T_KEY GetKey(const ENTRY& entry) { return entry.key(); }
  static void SetKey(ENTRY& entry, CF<T_KEY> key) { entry.set_key(key); }
};

template <typename ENTRY>
using KEY_ACCESSOR = KEY_ACCESSOR_IMPL<ENTRY, HasKeyMethod<ENTRY>(0)>;

template <typename ENTRY>
typename KEY_ACCESSOR<ENTRY>::T_KEY GetKey(const ENTRY& entry) {
  return KEY_ACCESSOR<ENTRY>::GetKey(entry);
}

template <typename ENTRY>
using ENTRY_KEY_TYPE = current::decay<typename KEY_ACCESSOR<ENTRY>::T_KEY>;

template <typename ENTRY>
void SetKey(ENTRY& entry, CF<ENTRY_KEY_TYPE<ENTRY>> key) {
  KEY_ACCESSOR<ENTRY>::SetKey(entry, key);
}

}  // namespace sfinae

template <typename T, int Q>
struct DictionaryGlobalDeleterPersister : Padawan {
  T key_to_erase;
  virtual ~DictionaryGlobalDeleterPersister() {}
  DictionaryGlobalDeleterPersister(T key_to_erase = T()) : key_to_erase(key_to_erase) {}
  template <typename A>
  void serialize(A& ar) {
    ar(CEREAL_NVP(key_to_erase));
  }
};

}  // namespace yoda

#endif  // SHERLOCK_YODA_CONTAINER_DICTIONARY_METAPROGRAMMING_H
