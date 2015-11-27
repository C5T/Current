/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef IRIS_H
#define IRIS_H

#include "../../Yoda/yoda.h"

struct LabeledFlower : yoda::Padawan {
  size_t key;
  union {
    struct {
      double SL;
      double SW;
      double PL;
      double PW;
    };
    double x[4];
  };
  std::string label;

  LabeledFlower() = default;
  LabeledFlower(const LabeledFlower&) = default;
  LabeledFlower(size_t key, double sl, double sw, double pl, double pw, const std::string& label)
      : key(key), SL(sl), SW(sw), PL(pl), PW(pw), label(label) {}

  template <typename A>
  void serialize(A& ar) {
    Padawan::serialize(ar);
    ar(CEREAL_NVP(key), CEREAL_NVP(SL), CEREAL_NVP(SW), CEREAL_NVP(PL), CEREAL_NVP(PW), CEREAL_NVP(label));
  }

  using DeleterPersister = yoda::DictionaryGlobalDeleterPersister<size_t, __COUNTER__>;
};
CEREAL_REGISTER_TYPE(LabeledFlower);
CEREAL_REGISTER_TYPE(LabeledFlower::DeleterPersister);

#endif  // IRIS_H
