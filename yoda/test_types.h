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

#ifndef SHERLOCK_YODA_TEST_TYPES_H
#define SHERLOCK_YODA_TEST_TYPES_H

#include "yoda.h"

using yoda::Padawan;

struct KeyValueEntry : Padawan {
  int key;
  double value;

  KeyValueEntry(const int key = 0, const double value = 0.0) : key(key), value(value) {}

  template <typename A>
  void serialize(A& ar) {
    Padawan::serialize(ar);
    ar(CEREAL_NVP(key), CEREAL_NVP(value));
  }

  // Confirm that Yoda works with non-copyable types, as long as they are `std::move()`-d in.
  // TODO(dkorolev): Sync up with Max on whether we need this. Chances are, we don't, at least now.
  // void operator=(KeyValueEntry&& rhs) {
  //   key = rhs.key;
  //   value = rhs.value;
  // }
  // KeyValueEntry(KeyValueEntry&&) = default;
  // KeyValueEntry(const KeyValueEntry&) = delete;
  // void operator=(const KeyValueEntry&) = delete;
};
CEREAL_REGISTER_TYPE(KeyValueEntry);

struct StringKVEntry : Padawan {
  std::string key;
  std::string foo;

  StringKVEntry(const std::string& key = "", const std::string& foo = "") : key(key), foo(foo) {}

  template <typename A>
  void serialize(A& ar) {
    Padawan::serialize(ar);
    ar(CEREAL_NVP(key), CEREAL_NVP(foo));
  }
};
CEREAL_REGISTER_TYPE(StringKVEntry);

struct MatrixCell : Padawan {
  size_t row;
  std::string col;
  int value;

  MatrixCell(const size_t row = 0, const std::string& col = std::string("0"), const int value = 0)
      : row(row), col(col), value(value) {}

  template <typename A>
  void serialize(A& ar) {
    ar(CEREAL_NVP(row), CEREAL_NVP(col), CEREAL_NVP(value));
  }
};
CEREAL_REGISTER_TYPE(MatrixCell);

#endif  // SHERLOCK_YODA_TEST_TYPES_H
