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

#ifndef SHERLOCK_YODA_API_KEYENTRY_EXCEPTIONS_H
#define SHERLOCK_YODA_API_KEYENTRY_EXCEPTIONS_H

#include "metaprogramming.h"

#include "../../exceptions.h"
#include "../../../../Bricks/exception.h"

namespace yoda {

using namespace sfinae;

// Exception types for non-existent keys.
// Cover exception type for all key types and templated, narrowed down exception types, one per entry key type.
struct KeyNotFoundCoverException : bricks::Exception {};

template <typename ENTRY>
struct KeyNotFoundException : KeyNotFoundCoverException {
  typedef ENTRY T_ENTRY;
  typedef ENTRY_KEY_TYPE<ENTRY> T_KEY;
  const T_KEY key;
  explicit KeyNotFoundException(const T_KEY& key) : key(key) {}
};

// Exception types for the existence of a particular key being a runtime error.
// Cover exception type for all key types and templated, narrowed down exception types, one per entry key type.
struct KeyAlreadyExistsCoverException : bricks::Exception {};

template <typename ENTRY>
struct KeyAlreadyExistsException : KeyAlreadyExistsCoverException {
  typedef ENTRY T_ENTRY;
  typedef ENTRY_KEY_TYPE<ENTRY> T_KEY;
  const T_KEY key;
  explicit KeyAlreadyExistsException(const T_KEY& key) : key(key) {}
};

}  // namespace yoda

#endif  // SHERLOCK_YODA_API_KEYENTRY_EXCEPTIONS_H
