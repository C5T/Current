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

#ifndef KARL_TEST_SERVICE_SCHEMA_H
#define KARL_TEST_SERVICE_SCHEMA_H

#include "../karl.h"

#include "../../TypeSystem/optional.h"
#include "../../TypeSystem/struct.h"

namespace karl_unittest {

CURRENT_STRUCT(Number) {
  CURRENT_FIELD(x, int64_t);
  CURRENT_FIELD(is_prime, Optional<bool>);
  CURRENT_CONSTRUCTOR(Number)(int64_t x = 0) : x(x) {}
};

}  // namespace karl_unittest

#endif  // KARL_TEST_SERVICE_SCHEMA_H
