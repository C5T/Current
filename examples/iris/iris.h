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

#ifndef EXAMPLES_IRIS_IRIS_H
#define EXAMPLES_IRIS_IRIS_H

#include "../../typesystem/struct.h"
#include "../../storage/storage.h"

CURRENT_STRUCT(LabeledFlower) {
  CURRENT_FIELD(key, uint64_t);

  CURRENT_FIELD(SL, double);
  CURRENT_FIELD(SW, double);
  CURRENT_FIELD(PL, double);
  CURRENT_FIELD(PW, double);
  CURRENT_FIELD(label, std::string);

  CURRENT_DEFAULT_CONSTRUCTOR(LabeledFlower) {}
  CURRENT_CONSTRUCTOR(LabeledFlower)
  (size_t key, double sl, double sw, double pl, double pw, const std::string& label)
      : key(key), SL(sl), SW(sw), PL(pl), PW(pw), label(label) {}
};

CURRENT_STORAGE_FIELD_ENTRY(OrderedDictionary, LabeledFlower, LabeledFlowersDictionary);
CURRENT_STORAGE(LabeledFlowersDB) { CURRENT_STORAGE_FIELD(flowers, LabeledFlowersDictionary); };

#endif  // EXAMPLES_IRIS_IRIS_H
